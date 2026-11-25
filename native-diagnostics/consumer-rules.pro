# NativeOperationException 由 JNI 按类名和构造函数描述符查找。
# 保留类名和构造函数，避免 R8 只看到字符串引用时误删或重命名。
-keep class com.android.ndk.samples.nativediagnostics.NativeOperationException {
    public <init>(...);
}
