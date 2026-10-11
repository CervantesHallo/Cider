/* WMA DMO regression: an exhausted decoder must not retain INCOMPLETE.
 * A previous output call sets this bit, then DirectShow reuses the descriptor.
 * Build: i686-w64-mingw32-gcc -O2 -o wma-dmo-empty-output.exe
 *   wma-dmo-empty-output.c -lole32 -lstrmiids -ldmoguids -luuid
 * Optional arguments: Wine mf/tests/wmaencdata.bin and wmadecdata.bin.
 * The no-input S_FALSE assertion targets Wine's documented implementation
 * branch; generic DMO implementations may also return S_OK with no output.
 */
#define COBJMACROS
#define CONST_VTABLE
#include <windows.h>
#include <dshow.h>
#include <dmo.h>
#include <wmcodecdsp.h>
#include <stdio.h>
#include <string.h>

struct buffer { IMediaBuffer iface; LONG refs; DWORD length; BYTE data[8192]; };
static struct buffer output, compressed;
static volatile LONG done;
static const GUID wma2_subtype = {0x161, 0, 0x10, {0x80, 0, 0, 0xaa, 0, 0x38, 0x9b, 0x71}};
static const GUID wma_decoder_clsid = {0x2eeb4adf, 0x4578, 0x4d10,
        {0xbc, 0xa7, 0xbb, 0x95, 0x5f, 0x56, 0x32, 0x0a}};

static HRESULT WINAPI query(IMediaBuffer *iface, REFIID iid, void **out)
{
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_IUnknown) && !IsEqualGUID(iid, &IID_IMediaBuffer)) return E_NOINTERFACE;
    *out = iface;
    IMediaBuffer_AddRef(iface);
    return S_OK;
}
static struct buffer *from_iface(IMediaBuffer *iface)
{
    return CONTAINING_RECORD(iface, struct buffer, iface);
}
static ULONG WINAPI addref(IMediaBuffer *iface) { return InterlockedIncrement(&from_iface(iface)->refs); }
static ULONG WINAPI release(IMediaBuffer *iface) { return InterlockedDecrement(&from_iface(iface)->refs); }
static HRESULT WINAPI set_length(IMediaBuffer *iface, DWORD length)
{
    struct buffer *buffer = from_iface(iface);
    if (length > sizeof(buffer->data)) return E_INVALIDARG;
    buffer->length = length;
    return S_OK;
}
static HRESULT WINAPI get_length(IMediaBuffer *iface, DWORD *length)
{
    if (!length) return E_POINTER;
    *length = sizeof(from_iface(iface)->data);
    return S_OK;
}
static HRESULT WINAPI get_buffer(IMediaBuffer *iface, BYTE **data, DWORD *length)
{
    struct buffer *buffer = from_iface(iface);
    if (data) *data = buffer->data;
    if (length) *length = buffer->length;
    return S_OK;
}
static const IMediaBufferVtbl buffer_vtbl = {query, addref, release, set_length, get_length, get_buffer};

static DWORD WINAPI watchdog(void *unused)
{
    DWORD started = GetTickCount();
    (void)unused;
    while (!InterlockedCompareExchange(&done, 0, 0))
    {
        if (GetTickCount() - started > 20000)
        {
            fprintf(stderr, "TIMEOUT\n"); fflush(stderr);
            TerminateProcess(GetCurrentProcess(), 124);
            return 124;
        }
        Sleep(20);
    }
    return 0;
}

int main(int argc, char **argv)
{
    IMediaObject *dmo = NULL;
    HRESULT hr;
    HANDLE timer;
    int result = 1;
    BYTE input_format[sizeof(WAVEFORMATEX) + 10] = {0};
    WAVEFORMATEX *input = (WAVEFORMATEX *)input_format;
    WAVEFORMATEX pcm = {WAVE_FORMAT_PCM, 2, 22050, 88200, 4, 16, 0};
    DMO_MEDIA_TYPE input_type = {0}, output_type = {0};
    BYTE expected[8192];
    static const BYTE codec_data[10] = {0, 0x44, 0, 0, 0x17, 0, 0, 0, 0, 0};

    if (argc != 1 && argc != 3) return 2;
    if (argc == 3)
    {
        FILE *file = fopen(argv[1], "rb");
        if (!file) return 2;
        size_t length = fread(compressed.data, 1, 1487, file);
        int failed = ferror(file);
        fclose(file);
        if (failed || length != 1487) return 2;
        compressed.length = (DWORD)length;
        file = fopen(argv[2], "rb");
        if (!file) return 2;
        length = fread(expected, 1, sizeof(expected), file);
        failed = ferror(file);
        fclose(file);
        if (failed || length != sizeof(expected)) return 2;
    }
    output.iface.lpVtbl = &buffer_vtbl;
    compressed.iface.lpVtbl = &buffer_vtbl;
    output.refs = compressed.refs = 1;
    timer = CreateThread(NULL, 0, watchdog, NULL, 0, NULL);
    if (!timer) return 2;
    hr = CoInitialize(NULL);
    if (FAILED(hr)) goto finish;
    hr = CoCreateInstance(&wma_decoder_clsid, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMediaObject, (void **)&dmo);
    if (FAILED(hr)) goto cleanup;
    input->wFormatTag = 0x161;
    input->nChannels = 2;
    input->nSamplesPerSec = 22050;
    input->nAvgBytesPerSec = 4003;
    input->nBlockAlign = 1487;
    input->wBitsPerSample = 16;
    input->cbSize = 10;
    memcpy(input_format + sizeof(WAVEFORMATEX), codec_data, sizeof(codec_data));
    input_type.majortype = MEDIATYPE_Audio;
    input_type.subtype = wma2_subtype;
    input_type.formattype = FORMAT_WaveFormatEx;
    input_type.bTemporalCompression = TRUE;
    input_type.cbFormat = sizeof(input_format);
    input_type.pbFormat = input_format;
    output_type.majortype = MEDIATYPE_Audio;
    output_type.subtype = MEDIASUBTYPE_PCM;
    output_type.formattype = FORMAT_WaveFormatEx;
    output_type.bFixedSizeSamples = TRUE;
    output_type.lSampleSize = pcm.nBlockAlign;
    output_type.cbFormat = sizeof(pcm);
    output_type.pbFormat = (BYTE *)&pcm;
    hr = IMediaObject_SetInputType(dmo, 0, &input_type, 0);
    printf("SetInputType=%#lx\n", hr); fflush(stdout);
    if (FAILED(hr)) goto cleanup;
    hr = IMediaObject_SetOutputType(dmo, 0, &output_type, 0);
    printf("SetOutputType=%#lx\n", hr); fflush(stdout);
    if (FAILED(hr)) goto cleanup;
    result = 0;
    for (unsigned int i = 0; i < 32; ++i)
    {
        DMO_OUTPUT_DATA_BUFFER data = {&output.iface, (i & 1) ? 0x01000007 : DMO_OUTPUT_DATA_BUFFERF_INCOMPLETE, 0, 0};
        DWORD status = 0;
        output.length = 0;
        hr = IMediaObject_ProcessOutput(dmo, 0, 1, &data, &status);
        printf("empty=%u hr=%#lx flags=%#lx length=%lu\n", i + 1, hr, data.dwStatus, output.length);
        fflush(stdout);
        if (hr != S_FALSE || data.dwStatus || output.length)
        {
            result = 1;
            break;
        }
    }
    if (!result && argc == 3)
    {
        DMO_OUTPUT_DATA_BUFFER data = {&output.iface, 0, 0, 0};
        DWORD bytes = 0, calls = 0;
        BOOL exhausted = FALSE;
        hr = IMediaObject_ProcessInput(dmo, 0, &compressed.iface,
                DMO_INPUT_DATA_BUFFERF_SYNCPOINT | DMO_INPUT_DATA_BUFFERF_TIME, 0, 0);
        if (FAILED(hr)) { result = 1; goto cleanup; }
        /* Reuse the actual successful descriptor, as the DMO wrapper does. */
        for (; calls < 16; ++calls)
        {
            DWORD status = 0;
            output.length = 0;
            hr = IMediaObject_ProcessOutput(dmo, 0, 1, &data, &status);
            printf("decode=%lu hr=%#lx flags=%#lx length=%lu\n", calls + 1, hr, data.dwStatus, output.length);
            fflush(stdout);
            if (FAILED(hr) || output.length % pcm.nBlockAlign) { result = 1; break; }
            if ((data.dwStatus & DMO_OUTPUT_DATA_BUFFERF_TIMELENGTH) && data.rtTimelength <= 0)
            { result = 1; break; }
            if (!output.length)
            {
                exhausted = !(data.dwStatus & DMO_OUTPUT_DATA_BUFFERF_INCOMPLETE);
                break;
            }
            if (!bytes)
            {
                unsigned long long difference = 0;
                BOOL nonzero = FALSE;
                if (output.length != sizeof(expected)) { result = 1; break; }
                for (DWORD j = 0; j < output.length; j += 2)
                {
                    SHORT actual, reference;
                    memcpy(&actual, output.data + j, 2);
                    memcpy(&reference, expected + j, 2);
                    if (actual) nonzero = TRUE;
                    int delta = (int)actual - reference;
                    difference += delta < 0 ? (unsigned int)-delta : (unsigned int)delta;
                }
                /* Wine's fixture comparison metric; decoded output varies
                 * slightly with the bundled FFmpeg version. */
                unsigned long long diff_percent = difference * 100 / 65536 / (output.length / 2);
                printf("pcm_reference_diff_percent=%llu\n", diff_percent);
                if (diff_percent > 10 || !nonzero) { result = 1; break; }
            }
            bytes += output.length;
        }
        printf("decoded_bytes=%lu exhausted=%d\n", bytes, exhausted);
        if (bytes < 8192 || bytes > 32768 || !exhausted || compressed.refs != 1) result = 1;
    }
cleanup:
    if (dmo) IMediaObject_Release(dmo);
    CoUninitialize();
finish:
    if (result) fprintf(stderr, "failed hr=%#lx\n", hr);
    InterlockedExchange(&done, 1);
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
    return result;
}
