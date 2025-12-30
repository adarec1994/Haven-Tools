#include "ui_internal.h"

#ifdef _WIN32
#include <mmreg.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <propvarutil.h>
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#endif

void scanAudioFiles(AppState& state) {
    state.audioFiles.clear();
    state.voiceOverFiles.clear();
    if (state.selectedFolder.empty()) return;
    
    fs::path basePath = fs::path(state.selectedFolder);
    
    fs::path audioPath = basePath / "modules" / "single player" / "audio" / "sound";
    if (fs::exists(audioPath)) {
        for (const auto& entry : fs::recursive_directory_iterator(audioPath)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".fsb") {
                    state.audioFiles.push_back(entry.path().string());
                }
            }
        }
    }
    
    fs::path voPath = basePath / "modules" / "single player" / "audio" / "vo" / "en-us" / "vo";
    if (fs::exists(voPath)) {
        for (const auto& entry : fs::recursive_directory_iterator(voPath)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".fsb") {
                    state.voiceOverFiles.push_back(entry.path().string());
                }
            }
        }
    }
    
    state.audioFilesLoaded = true;
}

bool extractFSB4toMP3(const std::string& fsbPath, const std::string& outPath) {
    std::ifstream f(fsbPath, std::ios::binary);
    if (!f) return false;
    
    f.seekg(0, std::ios::end);
    size_t fileSize = f.tellg();
    f.seekg(0, std::ios::beg);
    
    if (fileSize < 0x80) return false;
    
    std::vector<uint8_t> data(fileSize);
    f.read(reinterpret_cast<char*>(data.data()), fileSize);
    
    if (data[0] != 'F' || data[1] != 'S' || data[2] != 'B' || data[3] != '4') {
        return false;
    }
    
    uint32_t numSamples = *reinterpret_cast<uint32_t*>(&data[4]);
    uint32_t headerSize = *reinterpret_cast<uint32_t*>(&data[8]);
    uint32_t dataSize = *reinterpret_cast<uint32_t*>(&data[12]);
    uint32_t mode = *reinterpret_cast<uint32_t*>(&data[16]);
    
    if (numSamples != 1) {
        return false;
    }
    
    if (!(mode & 0x00040000)) {
        return false;
    }
    
    size_t mp3Start = 0x30 + headerSize;
    if (mp3Start + 2 > fileSize) return false;
    
    if (data[mp3Start] != 0xFF || (data[mp3Start + 1] & 0xE0) != 0xE0) {
        return false;
    }
    
    if (mp3Start + dataSize > fileSize) {
        dataSize = fileSize - mp3Start;
    }
    
    std::ofstream out(outPath, std::ios::binary);
    if (!out) return false;
    
    out.write(reinterpret_cast<char*>(&data[mp3Start]), dataSize);
    return true;
}

std::vector<uint8_t> extractFSB4toMP3Data(const std::string& fsbPath) {
    std::vector<uint8_t> result;
    std::ifstream f(fsbPath, std::ios::binary);
    if (!f) return result;
    
    f.seekg(0, std::ios::end);
    size_t fileSize = f.tellg();
    f.seekg(0, std::ios::beg);
    
    if (fileSize < 0x80) return result;
    
    std::vector<uint8_t> data(fileSize);
    f.read(reinterpret_cast<char*>(data.data()), fileSize);
    
    if (data[0] != 'F' || data[1] != 'S' || data[2] != 'B' || data[3] != '4') {
        return result;
    }
    
    uint32_t numSamples = *reinterpret_cast<uint32_t*>(&data[4]);
    uint32_t headerSize = *reinterpret_cast<uint32_t*>(&data[8]);
    uint32_t dataSize = *reinterpret_cast<uint32_t*>(&data[12]);
    uint32_t mode = *reinterpret_cast<uint32_t*>(&data[16]);
    
    if (numSamples != 1) {
        return result;
    }
    
    if (!(mode & 0x00040000)) {
        return result;
    }
    
    size_t mp3Start = 0x30 + headerSize;
    if (mp3Start + 2 > fileSize) return result;
    
    if (data[mp3Start] != 0xFF || (data[mp3Start + 1] & 0xE0) != 0xE0) {
        return result;
    }
    
    if (mp3Start + dataSize > fileSize) {
        dataSize = fileSize - mp3Start;
    }
    
    result.assign(data.begin() + mp3Start, data.begin() + mp3Start + dataSize);
    return result;
}

std::string getFSB4SampleName(const std::string& fsbPath) {
    std::ifstream f(fsbPath, std::ios::binary);
    if (!f) return "";
    
    std::vector<uint8_t> header(0x60);
    f.read(reinterpret_cast<char*>(header.data()), 0x60);
    
    if (header[0] != 'F' || header[1] != 'S' || header[2] != 'B' || header[3] != '4') {
        return "";
    }
    
    std::string name;
    for (int i = 0x32; i < 0x60 && header[i] != 0; i++) {
        name += (char)header[i];
    }
    return name;
}

#ifdef _WIN32
static IMFSourceReader* g_pReader = nullptr;
static std::vector<uint8_t> g_audioBuffer;
static size_t g_audioBufferPos = 0;
static WAVEFORMATEX g_waveFormat = {0};
static HWAVEOUT g_hWaveOut = nullptr;
static WAVEHDR g_waveHdr = {0};
static bool g_audioPlaying = false;
static bool g_mfInitialized = false;
static std::vector<uint8_t> g_currentMP3Data;
static int g_audioDurationMs = 0;
static bool g_audioPaused = false;

class MemoryStream : public IStream {
public:
    MemoryStream(const uint8_t* data, size_t size) : m_data(data), m_size(size), m_pos(0), m_ref(1) {}
    
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IStream || riid == IID_ISequentialStream) {
            *ppv = static_cast<IStream*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&m_ref); }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG ref = InterlockedDecrement(&m_ref);
        if (ref == 0) delete this;
        return ref;
    }
    HRESULT STDMETHODCALLTYPE Read(void* pv, ULONG cb, ULONG* pcbRead) override {
        size_t toRead = (std::min)((size_t)cb, m_size - m_pos);
        memcpy(pv, m_data + m_pos, toRead);
        m_pos += toRead;
        if (pcbRead) *pcbRead = (ULONG)toRead;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Write(const void*, ULONG, ULONG*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER move, DWORD origin, ULARGE_INTEGER* newPos) override {
        switch (origin) {
            case STREAM_SEEK_SET: m_pos = (size_t)move.QuadPart; break;
            case STREAM_SEEK_CUR: m_pos += (size_t)move.QuadPart; break;
            case STREAM_SEEK_END: m_pos = m_size + (size_t)move.QuadPart; break;
        }
        if (m_pos > m_size) m_pos = m_size;
        if (newPos) newPos->QuadPart = m_pos;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CopyTo(IStream*, ULARGE_INTEGER, ULARGE_INTEGER*, ULARGE_INTEGER*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Commit(DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE Revert() override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Stat(STATSTG* pstat, DWORD) override {
        memset(pstat, 0, sizeof(*pstat));
        pstat->type = STGTY_STREAM;
        pstat->cbSize.QuadPart = m_size;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Clone(IStream**) override { return E_NOTIMPL; }
private:
    const uint8_t* m_data;
    size_t m_size;
    size_t m_pos;
    LONG m_ref;
};

void stopAudio() {
    if (g_hWaveOut) {
        waveOutReset(g_hWaveOut);
        waveOutUnprepareHeader(g_hWaveOut, &g_waveHdr, sizeof(g_waveHdr));
        waveOutClose(g_hWaveOut);
        g_hWaveOut = nullptr;
    }
    if (g_pReader) {
        g_pReader->Release();
        g_pReader = nullptr;
    }
    g_audioPlaying = false;
    g_audioPaused = false;
    g_audioBuffer.clear();
}

bool playAudioFromMemory(const std::vector<uint8_t>& mp3Data) {
    stopAudio();
    
    if (!g_mfInitialized) {
        if (FAILED(MFStartup(MF_VERSION))) return false;
        g_mfInitialized = true;
    }
    
    g_currentMP3Data = mp3Data;
    
    MemoryStream* pStream = new MemoryStream(g_currentMP3Data.data(), g_currentMP3Data.size());
    IMFByteStream* pByteStream = nullptr;
    if (FAILED(MFCreateMFByteStreamOnStream(pStream, &pByteStream))) {
        pStream->Release();
        return false;
    }
    pStream->Release();
    
    IMFAttributes* pAttrs = nullptr;
    MFCreateAttributes(&pAttrs, 1);
    
    if (FAILED(MFCreateSourceReaderFromByteStream(pByteStream, pAttrs, &g_pReader))) {
        pByteStream->Release();
        if (pAttrs) pAttrs->Release();
        return false;
    }
    pByteStream->Release();
    if (pAttrs) pAttrs->Release();
    
    IMFMediaType* pPartialType = nullptr;
    MFCreateMediaType(&pPartialType);
    pPartialType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    pPartialType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    g_pReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pPartialType);
    pPartialType->Release();
    
    IMFMediaType* pUncompType = nullptr;
    g_pReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pUncompType);
    
    UINT32 channels = 0, sampleRate = 0, bitsPerSample = 0;
    pUncompType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);
    pUncompType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sampleRate);
    pUncompType->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bitsPerSample);
    pUncompType->Release();
    
    PROPVARIANT var;
    PropVariantInit(&var);
    if (SUCCEEDED(g_pReader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &var))) {
        g_audioDurationMs = (int)(var.uhVal.QuadPart / 10000);
        PropVariantClear(&var);
    }
    
    g_audioBuffer.clear();
    while (true) {
        IMFSample* pSample = nullptr;
        DWORD flags = 0;
        HRESULT hr = g_pReader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, nullptr, &pSample);
        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
            if (pSample) pSample->Release();
            break;
        }
        if (pSample) {
            IMFMediaBuffer* pBuffer = nullptr;
            pSample->ConvertToContiguousBuffer(&pBuffer);
            BYTE* pData = nullptr;
            DWORD cbData = 0;
            pBuffer->Lock(&pData, nullptr, &cbData);
            size_t oldSize = g_audioBuffer.size();
            g_audioBuffer.resize(oldSize + cbData);
            memcpy(g_audioBuffer.data() + oldSize, pData, cbData);
            pBuffer->Unlock();
            pBuffer->Release();
            pSample->Release();
        }
    }
    
    g_pReader->Release();
    g_pReader = nullptr;
    
    if (g_audioBuffer.empty()) return false;
    
    g_waveFormat.wFormatTag = WAVE_FORMAT_PCM;
    g_waveFormat.nChannels = (WORD)channels;
    g_waveFormat.nSamplesPerSec = sampleRate;
    g_waveFormat.wBitsPerSample = (WORD)bitsPerSample;
    g_waveFormat.nBlockAlign = g_waveFormat.nChannels * g_waveFormat.wBitsPerSample / 8;
    g_waveFormat.nAvgBytesPerSec = g_waveFormat.nSamplesPerSec * g_waveFormat.nBlockAlign;
    g_waveFormat.cbSize = 0;
    
    if (waveOutOpen(&g_hWaveOut, WAVE_MAPPER, &g_waveFormat, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        return false;
    }
    
    memset(&g_waveHdr, 0, sizeof(g_waveHdr));
    g_waveHdr.lpData = (LPSTR)g_audioBuffer.data();
    g_waveHdr.dwBufferLength = (DWORD)g_audioBuffer.size();
    waveOutPrepareHeader(g_hWaveOut, &g_waveHdr, sizeof(g_waveHdr));
    waveOutWrite(g_hWaveOut, &g_waveHdr, sizeof(g_waveHdr));
    
    g_audioPlaying = true;
    g_audioPaused = false;
    
    return true;
}

bool isAudioPlaying() {
    if (!g_hWaveOut || !g_audioPlaying) return false;
    if (g_audioPaused) return false;
    return (g_waveHdr.dwFlags & WHDR_DONE) == 0;
}

int getAudioLength() {
    return g_audioDurationMs;
}

int getAudioPosition() {
    if (!g_hWaveOut || !g_audioPlaying) return 0;
    if (g_audioPaused) {
        return (int)g_audioBufferPos;
    }
    MMTIME mmt;
    mmt.wType = TIME_MS;
    if (waveOutGetPosition(g_hWaveOut, &mmt, sizeof(mmt)) == MMSYSERR_NOERROR) {
        return mmt.u.ms;
    }
    return 0;
}

void setAudioPosition(int ms) {
    (void)ms;
}

void pauseAudio() {
    if (g_hWaveOut && g_audioPlaying) {
        g_audioBufferPos = getAudioPosition();
        waveOutPause(g_hWaveOut);
        g_audioPaused = true;
    }
}

void resumeAudio() {
    if (g_hWaveOut && g_audioPlaying && g_audioPaused) {
        waveOutRestart(g_hWaveOut);
        g_audioPaused = false;
    }
}

bool playAudio(const std::string& mp3Path) {
    std::ifstream f(mp3Path, std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    size_t size = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(size);
    f.read(reinterpret_cast<char*>(data.data()), size);
    return playAudioFromMemory(data);
}

#else
void stopAudio() {}
bool playAudioFromMemory(const std::vector<uint8_t>&) { return false; }
bool playAudio(const std::string&) { return false; }
bool isAudioPlaying() { return false; }
int getAudioLength() { return 0; }
int getAudioPosition() { return 0; }
void setAudioPosition(int) {}
void pauseAudio() {}
void resumeAudio() {}
#endif
