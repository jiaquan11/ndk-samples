package com.android.ndk.samples.nativediagnostics;

import java.util.Arrays;
import java.util.Objects;

/**
 * Android 为历史进程退出保存的一条只读诊断记录。
 *
 * <p>该类型只复制 {@code ApplicationExitInfo} 中与 Native 崩溃和 ANR
 * 诊断直接相关的稳定事实，不保存 Framework 对象，也不负责去重、落盘或上传。
 */
public final class ProcessExitRecord {
    /** 当前轻量采集器关注的 Native crash 与退出原因为 ANR 的两类记录。 */
    public enum Kind {
        NATIVE_CRASH,
        ANR
    }

    /** 原始 trace 的读取结果。 */
    public enum TraceStatus {
        /** 系统未保留 trace，或该 Android 版本不提供对应内容。 */
        NOT_AVAILABLE,
        /** trace 已在调用方给定的大小上限内完整读取。 */
        COMPLETE,
        /** trace 超过调用方给定上限，只保留上限范围内的内容前缀。 */
        TRUNCATED,
        /** 打开或读取原始 trace 时发生 I/O 错误。 */
        READ_FAILED
    }

    private final Kind kind;
    private final long timestampMillis;
    private final int pid;
    private final String processName;
    private final int platformReason;
    private final int status;
    private final int importance;
    private final long pssKb;
    private final long rssKb;
    private final String description;
    private final TraceStatus traceStatus;
    private final byte[] traceBytes;

    ProcessExitRecord(
            Kind kind,
            long timestampMillis,
            int pid,
            String processName,
            int platformReason,
            int status,
            int importance,
            long pssKb,
            long rssKb,
            String description,
            TraceStatus traceStatus,
            byte[] traceBytes) {
        this.kind = Objects.requireNonNull(kind, "kind");
        this.timestampMillis = timestampMillis;
        this.pid = pid;
        this.processName = Objects.requireNonNull(processName, "processName");
        this.platformReason = platformReason;
        this.status = status;
        this.importance = importance;
        this.pssKb = pssKb;
        this.rssKb = rssKb;
        this.description = description;
        this.traceStatus = Objects.requireNonNull(traceStatus, "traceStatus");

        final boolean traceShouldExist =
                traceStatus == TraceStatus.COMPLETE || traceStatus == TraceStatus.TRUNCATED;
        if (traceShouldExist != (traceBytes != null)) {
            throw new IllegalArgumentException("traceStatus and traceBytes do not match");
        }
        this.traceBytes = traceBytes == null
                ? null
                : Arrays.copyOf(traceBytes, traceBytes.length);
    }

    public Kind getKind() {
        return kind;
    }

    public long getTimestampMillis() {
        return timestampMillis;
    }

    public int getPid() {
        return pid;
    }

    public String getProcessName() {
        return processName;
    }

    /** 返回 Android 原始的 {@code ApplicationExitInfo.getReason()}。 */
    public int getPlatformReason() {
        return platformReason;
    }

    /**
     * 返回进程退出状态。对信号退出通常是信号编号；其它退出原因由 Android 定义。
     */
    public int getStatus() {
        return status;
    }

    public int getImportance() {
        return importance;
    }

    /** 返回系统最后一次采样到的 PSS，单位为 KiB，并非退出瞬间的精确值。 */
    public long getPssKb() {
        return pssKb;
    }

    /** 返回系统最后一次采样到的 RSS，单位为 KiB，并非退出瞬间的精确值。 */
    public long getRssKb() {
        return rssKb;
    }

    /**
     * 返回系统提供的人类可读说明；其格式可能随 Android 版本和设备变化。
     */
    public String getDescription() {
        return description;
    }

    public TraceStatus getTraceStatus() {
        return traceStatus;
    }

    /**
     * 返回原始 trace 的副本；未提供或读取失败时返回 {@code null}。
     *
     * <p>ANR 通常是文本 trace；Android 12/API 31 起，Native crash 对应的是
     * tombstone protobuf。采集器不会在端内解析这两种格式。
     */
    public byte[] getTraceBytes() {
        return traceBytes == null ? null : Arrays.copyOf(traceBytes, traceBytes.length);
    }

    public int getTraceSizeBytes() {
        return traceBytes == null ? 0 : traceBytes.length;
    }
}
