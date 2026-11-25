package com.example.unittest

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import com.android.ndk.samples.nativediagnostics.ProcessExitCollector
import com.android.ndk.samples.nativediagnostics.ProcessExitRecord
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class ProcessExitCollectorInstrumentedTest {
    @Test
    fun collectedRecordsRespectPublicBounds() {
        val context = InstrumentationRegistry.getInstrumentation().targetContext
        val records = ProcessExitCollector.collect(
            context,
            MAX_HISTORY_RECORDS,
            MAX_TRACE_BYTES,
        )

        if (!ProcessExitCollector.isSupported()) {
            assertTrue(records.isEmpty())
            return
        }

        assertTrue(records.size <= MAX_HISTORY_RECORDS)
        records.forEach { record ->
            assertTrue(
                record.kind == ProcessExitRecord.Kind.NATIVE_CRASH ||
                    record.kind == ProcessExitRecord.Kind.ANR,
            )
            assertTrue(record.timestampMillis >= 0)
            assertFalse(record.processName.isEmpty())
            assertTrue(record.traceSizeBytes <= MAX_TRACE_BYTES)

            val trace = record.traceBytes
            when (record.traceStatus) {
                ProcessExitRecord.TraceStatus.COMPLETE,
                ProcessExitRecord.TraceStatus.TRUNCATED
                -> assertEquals(record.traceSizeBytes, trace?.size)

                ProcessExitRecord.TraceStatus.NOT_AVAILABLE,
                ProcessExitRecord.TraceStatus.READ_FAILED
                -> assertNull(trace)

                null -> throw AssertionError("traceStatus must not be null")
            }
        }
    }

    private companion object {
        const val MAX_HISTORY_RECORDS = 8
        const val MAX_TRACE_BYTES = 64 * 1024
    }
}
