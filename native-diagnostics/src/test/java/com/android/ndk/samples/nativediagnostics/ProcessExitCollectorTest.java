package com.android.ndk.samples.nativediagnostics;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import org.junit.Test;

public class ProcessExitCollectorTest {
    @Test
    public void readTraceWithoutStreamReturnsNotAvailable() {
        final ProcessExitCollector.TraceReadResult result =
                ProcessExitCollector.readTrace(null, 4);

        assertEquals(ProcessExitRecord.TraceStatus.NOT_AVAILABLE, result.status);
        assertNull(result.bytes);
    }

    @Test
    public void readTraceWithinLimitReturnsCompleteBytes() {
        final byte[] source = {1, 2, 3};

        final ProcessExitCollector.TraceReadResult result =
                ProcessExitCollector.readTrace(new ByteArrayInputStream(source), 4);

        assertEquals(ProcessExitRecord.TraceStatus.COMPLETE, result.status);
        assertArrayEquals(source, result.bytes);
    }

    @Test
    public void readTraceAtLimitIsNotMarkedTruncated() {
        final byte[] source = {1, 2, 3, 4};

        final ProcessExitCollector.TraceReadResult result =
                ProcessExitCollector.readTrace(new ByteArrayInputStream(source), 4);

        assertEquals(ProcessExitRecord.TraceStatus.COMPLETE, result.status);
        assertArrayEquals(source, result.bytes);
    }

    @Test
    public void readTraceBeyondLimitKeepsPrefixAndMarksTruncated() {
        final byte[] source = {1, 2, 3, 4, 5};

        final ProcessExitCollector.TraceReadResult result =
                ProcessExitCollector.readTrace(new ByteArrayInputStream(source), 4);

        assertEquals(ProcessExitRecord.TraceStatus.TRUNCATED, result.status);
        assertArrayEquals(new byte[] {1, 2, 3, 4}, result.bytes);
    }

    @Test
    public void readTraceInitialFailureReturnsReadFailed() {
        final ProcessExitCollector.TraceReadResult result =
                ProcessExitCollector.readTrace(new ImmediateFailingInputStream(), 4);

        assertEquals(ProcessExitRecord.TraceStatus.READ_FAILED, result.status);
        assertNull(result.bytes);
    }

    @Test
    public void readTraceFailureDoesNotExposePartialBytes() {
        final ProcessExitCollector.TraceReadResult result =
                ProcessExitCollector.readTrace(new PartialThenFailingInputStream(), 4);

        assertEquals(ProcessExitRecord.TraceStatus.READ_FAILED, result.status);
        assertNull(result.bytes);
    }

    @Test
    public void recordDefensivelyCopiesTraceBytes() {
        final byte[] source = {7, 8};
        final ProcessExitRecord record = new ProcessExitRecord(
                ProcessExitRecord.Kind.NATIVE_CRASH,
                1L,
                2,
                "sample",
                5,
                11,
                100,
                3L,
                4L,
                "description",
                ProcessExitRecord.TraceStatus.COMPLETE,
                source);

        source[0] = 0;
        final byte[] firstRead = record.getTraceBytes();
        firstRead[1] = 0;

        assertArrayEquals(new byte[] {7, 8}, record.getTraceBytes());
    }

    private static final class ImmediateFailingInputStream extends InputStream {
        @Override
        public int read() throws IOException {
            throw new IOException("expected test failure");
        }
    }

    private static final class PartialThenFailingInputStream extends InputStream {
        private boolean returnedFirstByte;

        @Override
        public int read() throws IOException {
            if (!returnedFirstByte) {
                returnedFirstByte = true;
                return 1;
            }
            throw new IOException("expected test failure");
        }
    }
}
