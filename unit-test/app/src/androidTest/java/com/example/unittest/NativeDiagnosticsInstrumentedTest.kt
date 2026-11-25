package com.example.unittest

import androidx.test.ext.junit.runners.AndroidJUnit4
import com.android.ndk.samples.nativediagnostics.NativeOperationException
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class NativeDiagnosticsInstrumentedTest {

    @Before
    fun resetReporterBeforeTest() {
        NativeDiagnosticsBridge.clearReporter()
    }

    @After
    fun clearReporterAfterTest() {
        NativeDiagnosticsBridge.clearReporter()
    }

    @Test
    fun standardExceptionCreatesOneCorrelatedDiagnostic() {
        val exception = expectThrows<NativeOperationException> {
            NativeDiagnosticsBridge.throwStandard()
        }

        assertEquals("cpp", exception.nativeDomain)
        assertEquals(1003, exception.nativeCode)
        assertEquals("native-diagnostics-test", exception.nativeModule)
        assertEquals("throwStandard", exception.nativeOperation)
        assertEquals("A C++ runtime_error", exception.message)
        assertNotEquals(0L, exception.diagnosticId)
        assertEquals(exception.diagnosticId, NativeDiagnosticsBridge.lastReporterId())
        assertTrue(NativeDiagnosticsBridge.lastReporterStackWasJniCatchBoundary())
        assertTrue(NativeDiagnosticsBridge.lastReporterStackFrameCount() > 0)
    }

    @Test
    fun invalidArgumentMapsToIllegalArgumentException() {
        val exception = expectThrows<IllegalArgumentException> {
            NativeDiagnosticsBridge.throwInvalidArgument()
        }
        assertEquals("bad argument", exception.message)
    }

    @Test
    fun unknownCppExceptionUsesStableFallback() {
        val exception = expectThrows<NativeOperationException> {
            NativeDiagnosticsBridge.throwUnknown()
        }
        assertEquals(1099, exception.nativeCode)
        assertEquals("Unknown C++ exception", exception.message)
    }

    @Test
    fun domainExceptionPreservesFieldsAndConstructionStack() {
        val exception = expectThrows<NativeOperationException> {
            NativeDiagnosticsBridge.throwDomain()
        }

        assertEquals("decoder", exception.nativeDomain)
        assertEquals(2001, exception.nativeCode)
        assertEquals("decode failed", exception.message)
        assertEquals(exception.diagnosticId, NativeDiagnosticsBridge.lastReporterId())
        assertTrue(NativeDiagnosticsBridge.lastReporterStackWasExceptionConstruction())
        assertTrue(NativeDiagnosticsBridge.lastReporterStackFrameCount() > 0)
    }

    @Test
    fun existingPendingExceptionIsPreserved() {
        val exception = expectThrows<IllegalStateException> {
            NativeDiagnosticsBridge.preservePending()
        }
        assertEquals("original", exception.message)
    }

    @Test
    fun reporterFailureDoesNotReplaceBusinessException() {
        val exception = expectThrows<NativeOperationException> {
            NativeDiagnosticsBridge.throwWithFailingReporter()
        }
        assertEquals("business failure", exception.message)
        assertEquals(1, NativeDiagnosticsBridge.failingReporterInvocationCount())
    }

    @Test
    fun badAllocUsesMinimalOutOfMemoryPath() {
        expectThrows<OutOfMemoryError> {
            NativeDiagnosticsBridge.throwBadAlloc()
        }
        assertEquals(0L, NativeDiagnosticsBridge.lastReporterId())
    }

    @Test
    fun successfulCallDoesNotCreateDiagnosticEvent() {
        assertEquals(1, NativeDiagnosticsBridge.invokeSuccess())
        assertEquals(0L, NativeDiagnosticsBridge.lastReporterId())
    }

    @Test
    fun unicodeMessageSurvivesUtf16Conversion() {
        val exception = expectThrows<NativeOperationException> {
            NativeDiagnosticsBridge.throwUnicode()
        }
        assertEquals("中文😀", exception.message)
    }

    @Test
    fun invalidUtf8IsReplacedInsteadOfCrashing() {
        val exception = expectThrows<NativeOperationException> {
            NativeDiagnosticsBridge.throwInvalidUtf8()
        }
        assertEquals("bad�(", exception.message)
    }

    @Test
    fun longMessageIsSafelyTruncated() {
        val exception = expectThrows<NativeOperationException> {
            NativeDiagnosticsBridge.throwLongMessage()
        }
        val message = requireNotNull(exception.message)
        assertEquals(1024, message.length)
        assertTrue(message.all { it == 'a' })
    }

    @Test
    fun overflowingEmojiIsNotSplitIntoHalfSurrogate() {
        val exception = expectThrows<NativeOperationException> {
            NativeDiagnosticsBridge.throwMessageEndingWithOverflowingEmoji()
        }
        val message = requireNotNull(exception.message)
        assertEquals(1023, message.length)
        assertEquals(1023, message.codePointCount(0, message.length))
        assertTrue(message.all { it == 'a' })
    }

    private inline fun <reified T : Throwable> expectThrows(
        block: () -> Unit,
    ): T {
        try {
            block()
        } catch (throwable: Throwable) {
            if (throwable is T) return throwable
            throw AssertionError(
                "Expected ${T::class.java.name}, but caught ${throwable::class.java.name}",
                throwable,
            )
        }
        throw AssertionError("Expected ${T::class.java.name}, but no exception was thrown")
    }
}
