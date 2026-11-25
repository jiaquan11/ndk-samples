package com.android.ndk.samples.nativediagnostics;

import android.annotation.SuppressLint;
import android.app.ActivityManager;
import android.app.ApplicationExitInfo;
import android.content.Context;
import android.os.Build;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Objects;

/**
 * 读取 Android 为当前应用包保留的 Native crash，以及退出原因被归类为 ANR
 * 的历史记录。
 *
 * <p>这是无状态的事后读取器：它不会安装 signal handler，不会标记记录已消费，
 * 也不负责持久化或上传。调用方应在后台线程调用，并在记录被可靠保存或处理后
 * 自行完成去重。
 */
public final class ProcessExitCollector {
    private static final int TRACE_BUFFER_BYTES = 8 * 1024;

    private ProcessExitCollector() {}

    /** Android 11/API 30 起才提供历史进程退出记录。 */
    // collect() 自身已经完成版本隔离，调用方不需要借此方法解锁高版本类型。
    @SuppressLint("AnnotateVersionCheck")
    public static boolean isSupported() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.R;
    }

    /**
     * 查询系统最近保留的历史退出，只返回 Native crash 和退出原因为 ANR 的记录。
     *
     * @param context 当前应用的 Context
     * @param maxRecords 交给系统查询的最大历史记录数，必须大于 0
     * @param maxTraceBytes 每条原始 trace 最多复制的字节数，必须大于 0；调用方应同时
     *                      评估最多约 {@code maxRecords * maxTraceBytes} 的结果数据量
     * @return 按系统提供的从新到旧顺序排列的不可变列表；不支持或没有记录时为空
     */
    public static List<ProcessExitRecord> collect(
            Context context, int maxRecords, int maxTraceBytes) {
        Objects.requireNonNull(context, "context");
        if (maxRecords <= 0) {
            throw new IllegalArgumentException("maxRecords must be greater than 0");
        }
        if (maxTraceBytes <= 0) {
            throw new IllegalArgumentException("maxTraceBytes must be greater than 0");
        }
        if (!isSupported()) {
            return Collections.emptyList();
        }
        return Api30Impl.collect(context, maxRecords, maxTraceBytes);
    }

    // 高版本类型集中在该私有实现中，且只会在 collect() 的显式版本判断后进入。
    @SuppressLint("NewApi")
    private static final class Api30Impl {
        private Api30Impl() {}

        static List<ProcessExitRecord> collect(
                Context context, int maxRecords, int maxTraceBytes) {
            final ActivityManager activityManager =
                    (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
            if (activityManager == null) {
                return Collections.emptyList();
            }

            final List<ApplicationExitInfo> exitInfos =
                    activityManager.getHistoricalProcessExitReasons(
                            context.getPackageName(), 0, maxRecords);
            final List<ProcessExitRecord> records = new ArrayList<>(exitInfos.size());
            for (ApplicationExitInfo exitInfo : exitInfos) {
                final ProcessExitRecord.Kind kind = toSupportedKind(exitInfo.getReason());
                if (kind == null) {
                    continue;
                }

                final TraceReadResult trace = readPlatformTrace(exitInfo, maxTraceBytes);
                records.add(new ProcessExitRecord(
                        kind,
                        exitInfo.getTimestamp(),
                        exitInfo.getPid(),
                        exitInfo.getProcessName(),
                        exitInfo.getReason(),
                        exitInfo.getStatus(),
                        exitInfo.getImportance(),
                        exitInfo.getPss(),
                        exitInfo.getRss(),
                        exitInfo.getDescription(),
                        trace.status,
                        trace.bytes));
            }
            return Collections.unmodifiableList(records);
        }

        private static ProcessExitRecord.Kind toSupportedKind(int reason) {
            if (reason == ApplicationExitInfo.REASON_CRASH_NATIVE) {
                return ProcessExitRecord.Kind.NATIVE_CRASH;
            }
            if (reason == ApplicationExitInfo.REASON_ANR) {
                return ProcessExitRecord.Kind.ANR;
            }
            return null;
        }

        private static TraceReadResult readPlatformTrace(
                ApplicationExitInfo exitInfo, int maxTraceBytes) {
            try {
                return readTrace(exitInfo.getTraceInputStream(), maxTraceBytes);
            } catch (IOException ignored) {
                return TraceReadResult.readFailed();
            }
        }
    }

    /** 包级可见，便于对确定性的限长读取逻辑做普通 JVM 测试。 */
    static TraceReadResult readTrace(InputStream input, int maxTraceBytes) {
        if (input == null) {
            return TraceReadResult.notAvailable();
        }

        try (InputStream trace = input) {
            final ByteArrayOutputStream output =
                    new ByteArrayOutputStream(Math.min(maxTraceBytes, TRACE_BUFFER_BYTES));
            final byte[] buffer = new byte[Math.min(maxTraceBytes, TRACE_BUFFER_BYTES)];
            int total = 0;
            while (total < maxTraceBytes) {
                final int read = trace.read(
                        buffer, 0, Math.min(buffer.length, maxTraceBytes - total));
                if (read == -1) {
                    return TraceReadResult.complete(output.toByteArray());
                }
                // 标准阻塞流在请求长度大于 0 时通常不会返回 0；仍做一次单字节
                // 兜底，避免非标准实现让循环一直没有进展。
                if (read == 0) {
                    final int value = trace.read();
                    if (value == -1) {
                        return TraceReadResult.complete(output.toByteArray());
                    }
                    output.write(value);
                    total++;
                    continue;
                }
                output.write(buffer, 0, read);
                total += read;
            }

            final boolean truncated = trace.read() != -1;
            return truncated
                    ? TraceReadResult.truncated(output.toByteArray())
                    : TraceReadResult.complete(output.toByteArray());
        } catch (IOException ignored) {
            return TraceReadResult.readFailed();
        }
    }

    static final class TraceReadResult {
        final ProcessExitRecord.TraceStatus status;
        final byte[] bytes;

        private TraceReadResult(ProcessExitRecord.TraceStatus status, byte[] bytes) {
            this.status = status;
            this.bytes = bytes;
        }

        static TraceReadResult notAvailable() {
            return new TraceReadResult(ProcessExitRecord.TraceStatus.NOT_AVAILABLE, null);
        }

        static TraceReadResult complete(byte[] bytes) {
            return new TraceReadResult(ProcessExitRecord.TraceStatus.COMPLETE, bytes);
        }

        static TraceReadResult truncated(byte[] bytes) {
            return new TraceReadResult(ProcessExitRecord.TraceStatus.TRUNCATED, bytes);
        }

        static TraceReadResult readFailed() {
            return new TraceReadResult(ProcessExitRecord.TraceStatus.READ_FAILED, null);
        }
    }

}
