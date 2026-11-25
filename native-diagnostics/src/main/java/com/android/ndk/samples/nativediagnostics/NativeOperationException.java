package com.android.ndk.samples.nativediagnostics;

/**
 * Native 可恢复错误映射到 Java/Kotlin 层的稳定异常契约。
 *
 * <p>本异常携带业务处理所需的稳定错误字段，以及用于关联同一最终
 * Native .so 内日志与结构化事件的 diagnosticId。Native 栈只存在于对应的
 * NativeDiagnosticEvent 中，不复制进 Java 异常对象；当前事件也不包含
 * Build ID、应用版本或离线符号化结果。</p>
 */
public final class NativeOperationException extends RuntimeException {
    private static final long serialVersionUID = 1L;

    private final String nativeDomain;
    private final int nativeCode;
    private final String nativeModule;
    private final String nativeOperation;
    private final long diagnosticId;

    public NativeOperationException(
            String nativeDomain,
            int nativeCode,
            String nativeModule,
            String nativeOperation,
            long diagnosticId,
            String message) {
        super(message);
        this.nativeDomain = nativeDomain;
        this.nativeCode = nativeCode;
        this.nativeModule = nativeModule;
        this.nativeOperation = nativeOperation;
        this.diagnosticId = diagnosticId;
    }

    public String getNativeDomain() {
        return nativeDomain;
    }

    public int getNativeCode() {
        return nativeCode;
    }

    public String getNativeModule() {
        return nativeModule;
    }

    public String getNativeOperation() {
        return nativeOperation;
    }

    public long getDiagnosticId() {
        return diagnosticId;
    }
}
