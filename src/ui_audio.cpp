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

static const int ima_step_table[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

static const int ima_index_table[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
};

#ifdef _WIN32
bool decodeAudioToPCM(const std::vector<uint8_t>& inputData, std::vector<uint8_t>& outputPCM, uint32_t& channels, uint32_t& sampleRate, uint32_t& bitsPerSample);
#endif

void scanAudioFiles(AppState& state) {
    state.audioFiles.clear();
    state.voiceOverFiles.clear();
    if (state.selectedFolder.empty()) return;

    fs::path basePath = fs::path(state.selectedFolder);

    state.audioFiles.push_back("__HEADER__Core");

    fs::path coreSoundPath = basePath / "modules" / "single player" / "audio" / "sound";
    if (fs::exists(coreSoundPath)) {
        for (const auto& entry : fs::recursive_directory_iterator(coreSoundPath)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".fsb") state.audioFiles.push_back(entry.path().string());
            }
        }
    }
    fs::path pkgCoreSoundPath = basePath / "packages" / "core" / "audio" / "sound";
    if (fs::exists(pkgCoreSoundPath)) {
        for (const auto& entry : fs::recursive_directory_iterator(pkgCoreSoundPath)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".fsb") state.audioFiles.push_back(entry.path().string());
            }
        }
    }

    state.audioFiles.push_back("__HEADER__Awakening");
    fs::path awakSoundPath = basePath / "packages" / "core_ep1" / "audio" / "sound";
    if (fs::exists(awakSoundPath)) {
        for (const auto& entry : fs::recursive_directory_iterator(awakSoundPath)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".fsb") state.audioFiles.push_back(entry.path().string());
            }
        }
    }

    state.voiceOverFiles.push_back("__HEADER__Core");
    fs::path coreVoPath = basePath / "modules" / "single player" / "audio" / "vo" / "en-us" / "vo";
    if (fs::exists(coreVoPath)) {
        for (const auto& entry : fs::recursive_directory_iterator(coreVoPath)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".fsb") state.voiceOverFiles.push_back(entry.path().string());
            }
        }
    }
    fs::path pkgCoreVoPath = basePath / "packages" / "core" / "audio" / "vo" / "en-us" / "vo";
    if (fs::exists(pkgCoreVoPath)) {
        for (const auto& entry : fs::recursive_directory_iterator(pkgCoreVoPath)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".fsb") state.voiceOverFiles.push_back(entry.path().string());
            }
        }
    }

    state.voiceOverFiles.push_back("__HEADER__Awakening");
    fs::path awakVoPath = basePath / "packages" / "core_ep1" / "audio" / "vo" / "en-us" / "vo";
    if (fs::exists(awakVoPath)) {
        for (const auto& entry : fs::recursive_directory_iterator(awakVoPath)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".fsb") state.voiceOverFiles.push_back(entry.path().string());
            }
        }
    }

    state.audioFilesLoaded = true;
}

static std::vector<int16_t> decodeXboxImaAdpcm(const uint8_t* data, size_t dataLen, uint32_t numSamples) {
    std::vector<int16_t> output;
    output.reserve(numSamples);

    size_t offset = 0;
    uint32_t samplesDecoded = 0;

    while (samplesDecoded < numSamples && offset < dataLen) {
        if (offset + 4 > dataLen) break;
        int16_t predictor = (int16_t)(data[offset] | (data[offset + 1] << 8));
        int stepIndex = data[offset + 2];
        offset += 4;

        stepIndex = std::max(0, std::min(88, stepIndex));
        size_t nibbleBytes = std::min((size_t)32, dataLen - offset);

        for (size_t i = 0; i < nibbleBytes && samplesDecoded < numSamples; i++) {
            uint8_t byte = data[offset + i];
            for (int nibbleIdx = 0; nibbleIdx < 2 && samplesDecoded < numSamples; nibbleIdx++) {
                int nibble = (nibbleIdx == 0) ? (byte & 0x0F) : ((byte >> 4) & 0x0F);
                int step = ima_step_table[stepIndex];
                int diff = step >> 3;
                if (nibble & 1) diff += step >> 2;
                if (nibble & 2) diff += step >> 1;
                if (nibble & 4) diff += step;
                if (nibble & 8) diff = -diff;

                predictor += diff;
                predictor = std::max((int16_t)-32768, std::min((int16_t)32767, predictor));
                output.push_back(predictor);
                samplesDecoded++;
                stepIndex += ima_index_table[nibble];
                stepIndex = std::max(0, std::min(88, stepIndex));
            }
        }
        offset += nibbleBytes;
    }
    return output;
}

std::vector<FSBSampleInfo> parseFSB4Samples(const std::string& fsbPath) {
    std::vector<FSBSampleInfo> samples;
    std::ifstream f(fsbPath, std::ios::binary);
    if (!f) return samples;

    f.seekg(0, std::ios::end);
    size_t fileSize = f.tellg();
    f.seekg(0, std::ios::beg);

    if (fileSize < 0x30) return samples;
    std::vector<uint8_t> data(fileSize);
    f.read(reinterpret_cast<char*>(data.data()), fileSize);

    if (data[0] != 'F' || data[1] != 'S' || data[2] != 'B' || data[3] != '4') return samples;

    uint32_t numSamples = *reinterpret_cast<uint32_t*>(&data[4]);
    uint32_t sampleHeadersSize = *reinterpret_cast<uint32_t*>(&data[8]);
    uint32_t dataSize = *reinterpret_cast<uint32_t*>(&data[12]);

    const size_t FSB_HEADER_SIZE = 0x30;

    size_t headerSizePerSample = (numSamples > 0) ? (sampleHeadersSize / numSamples) : 0;
    if (headerSizePerSample < 0x40) headerSizePerSample = 0x50;

    size_t dataStart = FSB_HEADER_SIZE + sampleHeadersSize;
    size_t currentDataOffset = dataStart;

    for (uint32_t i = 0; i < numSamples; i++) {
        size_t headerOffset = FSB_HEADER_SIZE + (i * headerSizePerSample);
        if (headerOffset + 0x40 > fileSize) break;

        FSBSampleInfo info;
        char name[31] = {0};
        memcpy(name, &data[headerOffset + 2], 30);
        info.name = name;

        info.numSamples = *reinterpret_cast<uint32_t*>(&data[headerOffset + 0x20]);
        info.compressedSize = *reinterpret_cast<uint32_t*>(&data[headerOffset + 0x24]);
        info.mode = *reinterpret_cast<uint32_t*>(&data[headerOffset + 0x30]);
        info.sampleRate = *reinterpret_cast<uint32_t*>(&data[headerOffset + 0x34]);
        info.numChannels = *reinterpret_cast<uint16_t*>(&data[headerOffset + 0x3E]);

        info.dataOffset = currentDataOffset;
        currentDataOffset += info.compressedSize;

        if (info.sampleRate > 0) info.duration = (float)info.numSamples / info.sampleRate;
        else info.duration = 0.0f;

        samples.push_back(info);
    }
    return samples;
}

std::vector<uint8_t> extractFSB4toMP3Data(const std::string& fsbPath) {
    std::vector<uint8_t> result;
    auto samples = parseFSB4Samples(fsbPath);
    if (samples.empty()) return result;

    const auto& info = samples[0];

    bool isMpeg = (info.mode & 0x00040000) || (info.mode & 0x00000200);
    if (!isMpeg) return result;

    std::ifstream f(fsbPath, std::ios::binary);
    if (!f) return result;

    f.seekg(0, std::ios::end);
    size_t fileSize = f.tellg();
    f.seekg(0, std::ios::beg);

    if (info.dataOffset + info.compressedSize > fileSize) return result;

    result.resize(info.compressedSize);
    f.seekg(info.dataOffset);
    f.read(reinterpret_cast<char*>(result.data()), info.compressedSize);

    return result;
}

bool extractFSB4toMP3(const std::string& fsbPath, const std::string& outPath) {
    auto data = extractFSB4toMP3Data(fsbPath);
    if (data.empty()) return false;

    std::ofstream out(outPath, std::ios::binary);
    if (!out) return false;

    out.write(reinterpret_cast<char*>(data.data()), data.size());
    return true;
}

std::vector<uint8_t> extractFSB4SampleToWav(const std::string& fsbPath, int sampleIndex) {
    std::vector<uint8_t> wavData;

    auto samples = parseFSB4Samples(fsbPath);
    if (sampleIndex < 0 || sampleIndex >= (int)samples.size()) return wavData;
    const auto& info = samples[sampleIndex];

    std::ifstream f(fsbPath, std::ios::binary);
    if (!f) return wavData;
    f.seekg(0, std::ios::end);
    size_t fileSize = f.tellg();

    if (info.dataOffset + info.compressedSize > fileSize) return wavData;
    std::vector<uint8_t> rawData(info.compressedSize);
    f.seekg(info.dataOffset);
    f.read(reinterpret_cast<char*>(rawData.data()), info.compressedSize);

    bool isImaAdpcm = (info.mode & 0x00400000) != 0;
    bool isMp3 = (info.mode & 0x00040000) || (info.mode & 0x00000200);

    std::vector<int16_t> pcmData;
    uint32_t finalSampleRate = info.sampleRate;
    uint32_t finalChannels = info.numChannels;

    if (isImaAdpcm) {
        pcmData = decodeXboxImaAdpcm(rawData.data(), rawData.size(), info.numSamples);
    } else if (isMp3) {
        #ifdef _WIN32
        std::vector<uint8_t> pcmBytes;
        uint32_t decCh = 0, decRate = 0, decBits = 0;
        if (decodeAudioToPCM(rawData, pcmBytes, decCh, decRate, decBits)) {
            finalChannels = decCh;
            finalSampleRate = decRate;
            pcmData.resize(pcmBytes.size() / 2);
            memcpy(pcmData.data(), pcmBytes.data(), pcmBytes.size());
        }
        #endif
    }

    if (pcmData.empty()) return wavData;

    uint32_t dataSize = pcmData.size() * 2;
    uint32_t filesize = 36 + dataSize;
    uint16_t numChannels = finalChannels > 0 ? finalChannels : 1;
    uint16_t bitsPerSample = 16;
    uint32_t byteRate = finalSampleRate * numChannels * bitsPerSample / 8;
    uint16_t blockAlign = numChannels * bitsPerSample / 8;

    wavData.resize(44 + dataSize);
    uint8_t* wav = wavData.data();

    memcpy(wav, "RIFF", 4);
    *reinterpret_cast<uint32_t*>(wav + 4) = filesize;
    memcpy(wav + 8, "WAVE", 4);
    memcpy(wav + 12, "fmt ", 4);
    *reinterpret_cast<uint32_t*>(wav + 16) = 16;
    *reinterpret_cast<uint16_t*>(wav + 20) = 1;
    *reinterpret_cast<uint16_t*>(wav + 22) = numChannels;
    *reinterpret_cast<uint32_t*>(wav + 24) = finalSampleRate;
    *reinterpret_cast<uint32_t*>(wav + 28) = byteRate;
    *reinterpret_cast<uint16_t*>(wav + 32) = blockAlign;
    *reinterpret_cast<uint16_t*>(wav + 34) = bitsPerSample;
    memcpy(wav + 36, "data", 4);
    *reinterpret_cast<uint32_t*>(wav + 40) = dataSize;
    memcpy(wav + 44, pcmData.data(), dataSize);

    return wavData;
}

bool saveFSB4SampleToWav(const std::string& fsbPath, int sampleIndex, const std::string& outPath) {
    auto wavData = extractFSB4SampleToWav(fsbPath, sampleIndex);
    if (wavData.empty()) return false;
    std::ofstream out(outPath, std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<char*>(wavData.data()), wavData.size());
    return true;
}

std::string getFSB4SampleName(const std::string& fsbPath) {
    auto samples = parseFSB4Samples(fsbPath);
    if (!samples.empty()) return samples[0].name;
    return "";
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
static size_t g_playStartOffset = 0;

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

bool decodeAudioToPCM(const std::vector<uint8_t>& inputData, std::vector<uint8_t>& outputPCM, uint32_t& channels, uint32_t& sampleRate, uint32_t& bitsPerSample) {
    if (!g_mfInitialized) {
        if (FAILED(MFStartup(MF_VERSION))) return false;
        g_mfInitialized = true;
    }

    MemoryStream* pStream = new MemoryStream(inputData.data(), inputData.size());
    IMFByteStream* pByteStream = nullptr;
    if (FAILED(MFCreateMFByteStreamOnStream(pStream, &pByteStream))) {
        pStream->Release();
        return false;
    }
    pStream->Release();

    IMFAttributes* pAttrs = nullptr;
    MFCreateAttributes(&pAttrs, 1);
    IMFSourceReader* reader = nullptr;

    if (FAILED(MFCreateSourceReaderFromByteStream(pByteStream, pAttrs, &reader))) {
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
    reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pPartialType);
    pPartialType->Release();

    IMFMediaType* pUncompType = nullptr;
    reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pUncompType);

    UINT32 ch = 0, rate = 0, bits = 0;
    pUncompType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &ch);
    pUncompType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate);
    pUncompType->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bits);
    pUncompType->Release();

    channels = ch;
    sampleRate = rate;
    bitsPerSample = bits;

    outputPCM.clear();
    while (true) {
        IMFSample* pSample = nullptr;
        DWORD flags = 0;
        HRESULT hr = reader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, nullptr, &pSample);
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
            size_t oldSize = outputPCM.size();
            outputPCM.resize(oldSize + cbData);
            memcpy(outputPCM.data() + oldSize, pData, cbData);
            pBuffer->Unlock();
            pBuffer->Release();
            pSample->Release();
        }
    }
    reader->Release();
    return !outputPCM.empty();
}

void stopAudio() {
    if (g_hWaveOut) {
        waveOutReset(g_hWaveOut);
        waveOutUnprepareHeader(g_hWaveOut, &g_waveHdr, sizeof(g_waveHdr));
        waveOutClose(g_hWaveOut);
        g_hWaveOut = nullptr;
    }
    g_audioPlaying = false;
    g_audioPaused = false;
    g_audioBuffer.clear();
    g_playStartOffset = 0;
}

bool playAudioFromMemory(const std::vector<uint8_t>& mp3Data) {
    stopAudio();
    uint32_t ch, rate, bits;
    if (!decodeAudioToPCM(mp3Data, g_audioBuffer, ch, rate, bits)) return false;

    g_audioDurationMs = (int)((double)g_audioBuffer.size() / (rate * ch * (bits/8)) * 1000.0);

    g_waveFormat.wFormatTag = WAVE_FORMAT_PCM;
    g_waveFormat.nChannels = (WORD)ch;
    g_waveFormat.nSamplesPerSec = rate;
    g_waveFormat.wBitsPerSample = (WORD)bits;
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
    return true;
}

bool playWavFromMemory(const std::vector<uint8_t>& wavData) {
    stopAudio();
    if (wavData.size() < 44) return false;

    if (memcmp(wavData.data(), "RIFF", 4) != 0) return false;

    size_t pos = 12;
    uint16_t numChannels = 1;
    uint32_t sampleRate = 22050;
    uint16_t bitsPerSample = 16;
    size_t dataOffset = 0;
    size_t dataSize = 0;

    while (pos + 8 < wavData.size()) {
        char chunkId[5] = {0};
        memcpy(chunkId, &wavData[pos], 4);
        uint32_t chunkSize = *reinterpret_cast<const uint32_t*>(&wavData[pos + 4]);

        if (strcmp(chunkId, "fmt ") == 0) {
            numChannels = *reinterpret_cast<const uint16_t*>(&wavData[pos + 10]);
            sampleRate = *reinterpret_cast<const uint32_t*>(&wavData[pos + 12]);
            bitsPerSample = *reinterpret_cast<const uint16_t*>(&wavData[pos + 22]);
        } else if (strcmp(chunkId, "data") == 0) {
            dataOffset = pos + 8;
            dataSize = chunkSize;
            break;
        }
        pos += 8 + chunkSize;
    }

    if (dataOffset == 0 || dataSize == 0) return false;

    g_audioBuffer.assign(wavData.begin() + dataOffset, wavData.begin() + dataOffset + dataSize);
    g_audioDurationMs = (int)((dataSize * 1000) / (sampleRate * numChannels * (bitsPerSample / 8)));

    g_waveFormat.wFormatTag = WAVE_FORMAT_PCM;
    g_waveFormat.nChannels = numChannels;
    g_waveFormat.nSamplesPerSec = sampleRate;
    g_waveFormat.wBitsPerSample = bitsPerSample;
    g_waveFormat.nBlockAlign = numChannels * bitsPerSample / 8;
    g_waveFormat.nAvgBytesPerSec = sampleRate * g_waveFormat.nBlockAlign;
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
    return true;
}

bool isAudioPlaying() {
    if (!g_hWaveOut || !g_audioPlaying) return false;
    if (g_audioPaused) return false;
    return (g_waveHdr.dwFlags & WHDR_DONE) == 0;
}

int getAudioLength() { return g_audioDurationMs; }

int getAudioPosition() {
    if (!g_hWaveOut || !g_audioPlaying) return 0;
    if (g_audioPaused) {
        if (g_waveFormat.nAvgBytesPerSec > 0)
            return (int)((double)g_audioBufferPos / g_waveFormat.nAvgBytesPerSec * 1000.0);
        return 0;
    }
    MMTIME mmt;
    mmt.wType = TIME_BYTES;
    if (waveOutGetPosition(g_hWaveOut, &mmt, sizeof(mmt)) == MMSYSERR_NOERROR) {
        size_t totalBytes = mmt.u.cb + g_playStartOffset;
        if (g_waveFormat.nAvgBytesPerSec > 0) {
            return (int)((double)totalBytes / g_waveFormat.nAvgBytesPerSec * 1000.0);
        }
    }
    return 0;
}

void setAudioPosition(int ms) {
    if (!g_hWaveOut || ms < 0 || ms >= g_audioDurationMs) return;
    if (g_waveFormat.nAvgBytesPerSec == 0) return;
    size_t newBytePos = (size_t)((double)ms / 1000.0 * g_waveFormat.nAvgBytesPerSec);
    if (g_waveFormat.nBlockAlign > 0) newBytePos -= (newBytePos % g_waveFormat.nBlockAlign);

    if (newBytePos < g_audioBuffer.size()) {
        waveOutReset(g_hWaveOut);
        g_waveHdr.lpData = (LPSTR)(g_audioBuffer.data() + newBytePos);
        g_waveHdr.dwBufferLength = (DWORD)(g_audioBuffer.size() - newBytePos);
        g_playStartOffset = newBytePos;
        g_audioBufferPos = newBytePos;
        waveOutPrepareHeader(g_hWaveOut, &g_waveHdr, sizeof(g_waveHdr));
        waveOutWrite(g_hWaveOut, &g_waveHdr, sizeof(g_waveHdr));
        if (g_audioPaused) waveOutPause(g_hWaveOut);
    }
}

void pauseAudio() {
    if (g_hWaveOut && g_audioPlaying) {
        g_audioBufferPos = (size_t)((double)getAudioPosition() / 1000.0 * g_waveFormat.nAvgBytesPerSec);
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
bool playWavFromMemory(const std::vector<uint8_t>&) { return false; }
bool playAudio(const std::string&) { return false; }
bool isAudioPlaying() { return false; }
int getAudioLength() { return 0; }
int getAudioPosition() { return 0; }
void setAudioPosition(int) {}
void pauseAudio() {}
void resumeAudio() {}
#endif