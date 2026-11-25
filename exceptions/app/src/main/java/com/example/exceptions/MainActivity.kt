package com.example.exceptions

import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import com.android.ndk.samples.nativediagnostics.ProcessExitCollector
import com.android.ndk.samples.nativediagnostics.ProcessExitRecord
import com.example.exceptions.databinding.ActivityMainBinding
import java.text.DateFormat
import java.util.Date
import java.util.concurrent.Executors

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    private val diagnosticsExecutor = Executors.newSingleThreadExecutor()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        // Native 方法返回时，如果当前线程带有 pending RuntimeException，
        // ART 会沿 Java/Kotlin 调用栈派发该异常，成功分支不会继续执行。
        try {
            throwsException()
            binding.sampleText.text = getString(R.string.no_exception_thrown)
        } catch (e: java.lang.RuntimeException) {
            binding.sampleText.text = getString(
                R.string.runtime_exception_caught,
                e.message.orEmpty(),
            )
        }

        loadHistoricalProcessExits()
    }

    override fun onDestroy() {
        // 采集器不持有 Activity；这里只结束示例页面自己的后台执行器。
        diagnosticsExecutor.shutdownNow()
        super.onDestroy()
    }

    private fun loadHistoricalProcessExits() {
        if (!ProcessExitCollector.isSupported()) {
            binding.exitHistoryText.setText(R.string.exit_history_unsupported)
            return
        }

        binding.exitHistoryText.setText(R.string.exit_history_loading)
        diagnosticsExecutor.execute {
            var failure: RuntimeException? = null
            val records = try {
                ProcessExitCollector.collect(
                    applicationContext,
                    MAX_HISTORY_RECORDS,
                    MAX_TRACE_BYTES,
                )
            } catch (exception: RuntimeException) {
                failure = exception
                emptyList()
            }

            runOnUiThread {
                if (isDestroyed) return@runOnUiThread
                binding.exitHistoryText.text = if (failure == null) {
                    formatProcessExitRecords(records)
                } else {
                    getString(
                        R.string.exit_history_failed,
                        failure.javaClass.simpleName,
                    )
                }
            }
        }
    }

    private fun formatProcessExitRecords(records: List<ProcessExitRecord>): String {
        if (records.isEmpty()) return getString(R.string.exit_history_empty)

        val timeFormatter = DateFormat.getDateTimeInstance(
            DateFormat.SHORT,
            DateFormat.MEDIUM,
        )
        val details = records.take(MAX_DISPLAYED_RECORDS).joinToString("\n") { record ->
            getString(
                R.string.exit_history_item,
                record.kind.name,
                timeFormatter.format(Date(record.timestampMillis)),
                record.status,
                record.traceStatus.name,
                record.traceSizeBytes,
            )
        }
        return getString(
            R.string.exit_history_header,
            records.size,
            MAX_DISPLAYED_RECORDS,
        ) + "\n" + details
    }

    // 这里只声明 Native 实例方法；函数地址由 JNI_OnLoad() 动态注册。
    external fun throwsException()

    companion object {
        private const val MAX_HISTORY_RECORDS = 8
        private const val MAX_TRACE_BYTES = 1024 * 1024
        private const val MAX_DISPLAYED_RECORDS = 3

        // 加载 libexceptions.so，并触发 Native 层的 JNI_OnLoad()。
        init {
            System.loadLibrary("exceptions")
        }
    }
}
