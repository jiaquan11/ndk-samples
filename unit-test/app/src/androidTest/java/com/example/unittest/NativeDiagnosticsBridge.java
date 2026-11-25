package com.example.unittest;

final class NativeDiagnosticsBridge {
    static {
        System.loadLibrary("app_tests");
    }

    private NativeDiagnosticsBridge() {}

    static native void throwStandard();
    static native void throwInvalidArgument();
    static native void throwUnknown();
    static native void throwDomain();
    static native void preservePending();
    static native void throwWithFailingReporter();
    static native void throwBadAlloc();
    static native int invokeSuccess();
    static native void throwUnicode();
    static native void throwInvalidUtf8();
    static native void throwLongMessage();
    static native void throwMessageEndingWithOverflowingEmoji();

    static native long lastReporterId();
    static native int lastReporterStackFrameCount();
    static native boolean lastReporterStackWasExceptionConstruction();
    static native boolean lastReporterStackWasJniCatchBoundary();
    static native int failingReporterInvocationCount();
    static native void clearReporter();
}
