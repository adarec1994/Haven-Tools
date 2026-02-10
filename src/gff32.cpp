#include "Gff32.h"
#include <fstream>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <algorithm>
namespace GFF32 {
std::string ExoLocString::getDisplayValue() const {
    if (!strings.empty()) {
        return strings[0].text;
    }
    if (stringref >= 0) {
        return "StrRef:" + std::to_string(stringref);
    }
    return "";
}
std::string VoidData::getDisplayValue() const {
    std::ostringstream oss;
    oss << "(" << data.size() << " bytes)";
    if (data.size() <= 16) {
        oss << " [";
        for (size_t i = 0; i < data.size(); ++i) {
            if (i > 0) oss << " ";
            oss << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
        }
        oss << "]";
    }
    return oss.str();
}
std::string Field::getTypeName() const {
    return typeIdToString(typeId);
}
std::string Field::getDisplayValue() const {
    return fieldValueToString(value, typeId);
}
bool Field::isComplex() const {
    return typeId == TypeID::DWORD64 ||
           typeId == TypeID::INT64 ||
           typeId == TypeID::DOUBLE ||
           typeId == TypeID::ExoString ||
           typeId == TypeID::ResRef ||
           typeId == TypeID::ExoLocString ||
           typeId == TypeID::VOID ||
           typeId == TypeID::Structure ||
           typeId == TypeID::List;
}
bool Structure::hasField(const std::string& label) const {
    return fields.find(label) != fields.end();
}
const Field* Structure::getField(const std::string& label) const {
    auto it = fields.find(label);
    return it != fields.end() ? &it->second : nullptr;
}
Field* Structure::getField(const std::string& label) {
    auto it = fields.find(label);
    return it != fields.end() ? &it->second : nullptr;
}
void Structure::setField(const std::string& label, TypeID type, FieldValue value) {
    if (fields.find(label) == fields.end()) {
        fieldOrder.push_back(label);
    }
    fields[label] = Field{label, type, std::move(value)};
}
GFF32File::GFF32File() : m_loaded(false) {
    std::memset(&m_header, 0, sizeof(m_header));
}
GFF32File::~GFF32File() {
    close();
}
void GFF32File::close() {
    m_root.reset();
    m_loaded = false;
    std::memset(&m_header, 0, sizeof(m_header));
}
bool GFF32File::load(const std::string& path) {
    close();
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return false;
    size_t size = file.tellg();
    file.seekg(0);
    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    file.close();
    return load(data);
}
bool GFF32File::load(const std::vector<uint8_t>& data) {
    return load(data.data(), data.size());
}
bool GFF32File::load(const uint8_t* data, size_t size) {
    close();
    if (!parseHeader(data, size)) {
        return false;
    }
    if (!parseContent(data, size)) {
        return false;
    }
    m_loaded = true;
    return true;
}
bool GFF32File::parseHeader(const uint8_t* data, size_t size) {
    if (size < 56) return false;
    std::memcpy(m_header.fileType, data, 4);
    m_header.fileType[4] = '\0';
    std::memcpy(m_header.fileVersion, data + 4, 4);
    m_header.fileVersion[4] = '\0';
    if (std::string(m_header.fileVersion) != "V3.2") {
        return false;
    }
    m_header.structOffset = readAt<uint32_t>(data, 8);
    m_header.structCount = readAt<uint32_t>(data, 12);
    m_header.fieldOffset = readAt<uint32_t>(data, 16);
    m_header.fieldCount = readAt<uint32_t>(data, 20);
    m_header.labelOffset = readAt<uint32_t>(data, 24);
    m_header.labelCount = readAt<uint32_t>(data, 28);
    m_header.fieldDataOffset = readAt<uint32_t>(data, 32);
    m_header.fieldDataCount = readAt<uint32_t>(data, 36);
    m_header.fieldIndicesOffset = readAt<uint32_t>(data, 40);
    m_header.fieldIndicesCount = readAt<uint32_t>(data, 44);
    m_header.listIndicesOffset = readAt<uint32_t>(data, 48);
    m_header.listIndicesCount = readAt<uint32_t>(data, 52);
    return true;
}
bool GFF32File::parseContent(const uint8_t* data, size_t size) {
    std::vector<std::string> labels;
    labels.resize(m_header.labelCount);
    for (uint32_t i = 0; i < m_header.labelCount; ++i) {
        uint32_t offset = m_header.labelOffset + i * 16;
        if (offset + 16 > size) return false;
        char labelBuf[17];
        std::memcpy(labelBuf, data + offset, 16);
        labelBuf[16] = '\0';
        std::string label(labelBuf);
        size_t end = label.find('\0');
        if (end != std::string::npos) {
            label = label.substr(0, end);
        }
        labels[i] = label;
    }
    std::vector<Structure> structs;
    structs.resize(m_header.structCount);
    struct StructDef {
        uint32_t structId;
        uint32_t fieldOffset;
        uint32_t fieldCount;
    };
    std::vector<StructDef> structDefs(m_header.structCount);
    for (uint32_t i = 0; i < m_header.structCount; ++i) {
        uint32_t offset = m_header.structOffset + i * 12;
        if (offset + 12 > size) return false;
        structDefs[i].structId = readAt<uint32_t>(data, offset);
        structDefs[i].fieldOffset = readAt<uint32_t>(data, offset + 4);
        structDefs[i].fieldCount = readAt<uint32_t>(data, offset + 8);
        if (structDefs[i].structId == 0xFFFFFFFF) {
            structs[i].structId = -1;
        } else {
            structs[i].structId = static_cast<int32_t>(structDefs[i].structId);
        }
    }
    struct FieldDef {
        uint32_t typeId;
        uint32_t labelIndex;
        uint32_t dataOrOffset;
    };
    std::vector<FieldDef> fieldDefs(m_header.fieldCount);
    for (uint32_t i = 0; i < m_header.fieldCount; ++i) {
        uint32_t offset = m_header.fieldOffset + i * 12;
        if (offset + 12 > size) return false;
        fieldDefs[i].typeId = readAt<uint32_t>(data, offset);
        fieldDefs[i].labelIndex = readAt<uint32_t>(data, offset + 4);
        fieldDefs[i].dataOrOffset = readAt<uint32_t>(data, offset + 8);
    }
    auto getFieldIndices = [&](const StructDef& sd) -> std::vector<uint32_t> {
        std::vector<uint32_t> indices;
        if (sd.fieldCount == 0) {
            return indices;
        } else if (sd.fieldCount == 1) {
            indices.push_back(sd.fieldOffset);
        } else {
            for (uint32_t j = 0; j < sd.fieldCount; ++j) {
                uint32_t idxOffset = m_header.fieldIndicesOffset + sd.fieldOffset + j * 4;
                if (idxOffset + 4 <= size) {
                    indices.push_back(readAt<uint32_t>(data, idxOffset));
                }
            }
        }
        return indices;
    };
    const uint8_t* fieldData = data + m_header.fieldDataOffset;
    size_t fieldDataSize = size - m_header.fieldDataOffset;
    auto parseFieldValue = [&](const FieldDef& fd, std::vector<Structure>& structs) -> std::pair<TypeID, FieldValue> {
        TypeID type = static_cast<TypeID>(fd.typeId);
        FieldValue value;
        switch (type) {
            case TypeID::BYTE:
                value = static_cast<uint8_t>(fd.dataOrOffset & 0xFF);
                break;
            case TypeID::CHAR:
                value = static_cast<int8_t>(fd.dataOrOffset & 0xFF);
                break;
            case TypeID::WORD:
                value = static_cast<uint16_t>(fd.dataOrOffset & 0xFFFF);
                break;
            case TypeID::SHORT:
                value = static_cast<int16_t>(fd.dataOrOffset & 0xFFFF);
                break;
            case TypeID::DWORD:
                value = fd.dataOrOffset;
                break;
            case TypeID::INT:
                value = static_cast<int32_t>(fd.dataOrOffset);
                break;
            case TypeID::FLOAT: {
                float f;
                std::memcpy(&f, &fd.dataOrOffset, sizeof(float));
                value = f;
                break;
            }
            case TypeID::DWORD64: {
                if (fd.dataOrOffset + 8 <= fieldDataSize) {
                    uint64_t v;
                    std::memcpy(&v, fieldData + fd.dataOrOffset, 8);
                    value = v;
                } else {
                    value = uint64_t(0);
                }
                break;
            }
            case TypeID::INT64: {
                if (fd.dataOrOffset + 8 <= fieldDataSize) {
                    int64_t v;
                    std::memcpy(&v, fieldData + fd.dataOrOffset, 8);
                    value = v;
                } else {
                    value = int64_t(0);
                }
                break;
            }
            case TypeID::DOUBLE: {
                if (fd.dataOrOffset + 8 <= fieldDataSize) {
                    double d;
                    std::memcpy(&d, fieldData + fd.dataOrOffset, 8);
                    value = d;
                } else {
                    value = 0.0;
                }
                break;
            }
            case TypeID::ExoString: {
                if (fd.dataOrOffset + 4 <= fieldDataSize) {
                    uint32_t len = readAt<uint32_t>(fieldData, fd.dataOrOffset);
                    if (fd.dataOrOffset + 4 + len <= fieldDataSize) {
                        value = std::string(reinterpret_cast<const char*>(fieldData + fd.dataOrOffset + 4), len);
                    } else {
                        value = std::string();
                    }
                } else {
                    value = std::string();
                }
                break;
            }
            case TypeID::ResRef: {
                if (fd.dataOrOffset + 1 <= fieldDataSize) {
                    uint8_t len = fieldData[fd.dataOrOffset];
                    if (fd.dataOrOffset + 1 + len <= fieldDataSize) {
                        std::string s(reinterpret_cast<const char*>(fieldData + fd.dataOrOffset + 1), len);
                        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
                        value = s;
                    } else {
                        value = std::string();
                    }
                } else {
                    value = std::string();
                }
                break;
            }
            case TypeID::ExoLocString: {
                ExoLocString els;
                if (fd.dataOrOffset + 12 <= fieldDataSize) {
                    uint32_t totalSize = readAt<uint32_t>(fieldData, fd.dataOrOffset);
                    uint32_t strRef = readAt<uint32_t>(fieldData, fd.dataOrOffset + 4);
                    uint32_t strCount = readAt<uint32_t>(fieldData, fd.dataOrOffset + 8);
                    els.stringref = (strRef == 0xFFFFFFFF) ? -1 : static_cast<int32_t>(strRef);
                    uint32_t strOffset = fd.dataOrOffset + 12;
                    for (uint32_t s = 0; s < strCount && strOffset + 8 <= fieldDataSize; ++s) {
                        uint32_t strId = readAt<uint32_t>(fieldData, strOffset);
                        uint32_t strLen = readAt<uint32_t>(fieldData, strOffset + 4);
                        strOffset += 8;
                        if (strOffset + strLen <= fieldDataSize) {
                            LocalString ls;
                            ls.language = strId >> 2;
                            ls.gender = (strId & 1) != 0;
                            ls.text = std::string(reinterpret_cast<const char*>(fieldData + strOffset), strLen);
                            els.strings.push_back(ls);
                        }
                        strOffset += strLen;
                    }
                }
                value = els;
                break;
            }
            case TypeID::VOID: {
                VoidData vd;
                if (fd.dataOrOffset + 4 <= fieldDataSize) {
                    uint32_t len = readAt<uint32_t>(fieldData, fd.dataOrOffset);
                    if (fd.dataOrOffset + 4 + len <= fieldDataSize) {
                        vd.data.resize(len);
                        std::memcpy(vd.data.data(), fieldData + fd.dataOrOffset + 4, len);
                    }
                }
                value = vd;
                break;
            }
            case TypeID::Structure: {
                if (fd.dataOrOffset < structs.size()) {
                    auto ptr = std::make_shared<Structure>(structs[fd.dataOrOffset]);
                    value = ptr;
                } else {
                    value = std::make_shared<Structure>();
                }
                break;
            }
            case TypeID::List: {
                auto listPtr = std::make_shared<std::vector<Structure>>();
                if (fd.dataOrOffset + 4 <= m_header.listIndicesCount) {
                    uint32_t listPos = m_header.listIndicesOffset + fd.dataOrOffset;
                    if (listPos + 4 <= size) {
                        uint32_t count = readAt<uint32_t>(data, listPos);
                        listPos += 4;
                        for (uint32_t li = 0; li < count && listPos + 4 <= size; ++li) {
                            uint32_t structIdx = readAt<uint32_t>(data, listPos);
                            listPos += 4;
                            if (structIdx < structs.size()) {
                                listPtr->push_back(structs[structIdx]);
                            }
                        }
                    }
                }
                value = listPtr;
                break;
            }
            default:
                value = uint32_t(0);
                break;
        }
        return {type, value};
    };
    for (uint32_t i = 0; i < m_header.structCount; ++i) {
        auto fieldIndices = getFieldIndices(structDefs[i]);
        for (uint32_t fidx : fieldIndices) {
            if (fidx >= fieldDefs.size()) continue;
            const FieldDef& fd = fieldDefs[fidx];
            if (fd.labelIndex >= labels.size()) continue;
            const std::string& label = labels[fd.labelIndex];
            TypeID type = static_cast<TypeID>(fd.typeId);
            if (type == TypeID::Structure || type == TypeID::List) {
                continue;
            }
            auto [parsedType, parsedValue] = parseFieldValue(fd, structs);
            structs[i].setField(label, parsedType, std::move(parsedValue));
        }
    }
    for (uint32_t i = 0; i < m_header.structCount; ++i) {
        auto fieldIndices = getFieldIndices(structDefs[i]);
        for (uint32_t fidx : fieldIndices) {
            if (fidx >= fieldDefs.size()) continue;
            const FieldDef& fd = fieldDefs[fidx];
            if (fd.labelIndex >= labels.size()) continue;
            const std::string& label = labels[fd.labelIndex];
            TypeID type = static_cast<TypeID>(fd.typeId);
            if (type != TypeID::Structure && type != TypeID::List) {
                continue;
            }
            auto [parsedType, parsedValue] = parseFieldValue(fd, structs);
            structs[i].setField(label, parsedType, std::move(parsedValue));
        }
    }
    if (!structs.empty()) {
        m_root = std::make_shared<Structure>(structs[0]);
        m_root->fileType = m_header.fileType;
        m_root->fileVersion = m_header.fileVersion;
    }
    return true;
}
std::string GFF32File::fileType() const {
    if (m_root) return m_root->fileType;
    return std::string(m_header.fileType);
}
std::string GFF32File::fileVersion() const {
    if (m_root) return m_root->fileVersion;
    return std::string(m_header.fileVersion);
}
bool GFF32File::is2DA() const {
    return fileType() == "2DA ";
}
bool GFF32File::isDLG() const {
    return fileType() == "DLG ";
}
bool GFF32File::isUTI() const {
    return fileType() == "UTI ";
}
bool GFF32File::isUTC() const {
    return fileType() == "UTC ";
}
bool GFF32File::isUTP() const {
    return fileType() == "UTP ";
}
bool GFF32File::isGFF32(const std::vector<uint8_t>& data) {
    return isGFF32(data.data(), data.size());
}
bool GFF32File::isGFF32(const uint8_t* data, size_t size) {
    if (size < 8) return false;
    if (data[4] == 'V' && data[5] == '3' && data[6] == '.' && data[7] == '2') {
        return true;
    }
    return false;
}
bool GFF32File::save(const std::string& path) {
    auto data = save();
    if (data.empty()) return false;
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    return true;
}
struct WriterState {
    std::vector<uint8_t> structsBuf;
    std::vector<uint8_t> fieldsBuf;
    std::vector<uint8_t> labelsBuf;
    std::vector<uint8_t> fieldDataBuf;
    std::vector<uint8_t> fieldIndicesBuf;
    std::vector<uint8_t> listIndicesBuf;
    std::map<std::string, uint32_t> labelMap;
    uint32_t structCount = 0;
    uint32_t fieldCount = 0;

    template<typename T>
    static void append(std::vector<uint8_t>& buf, T val) {
        size_t pos = buf.size();
        buf.resize(pos + sizeof(T));
        std::memcpy(&buf[pos], &val, sizeof(T));
    }

    uint32_t getLabelIndex(const std::string& label) {
        auto it = labelMap.find(label);
        if (it != labelMap.end()) return it->second;
        uint32_t idx = static_cast<uint32_t>(labelMap.size());
        labelMap[label] = idx;
        char b[16];
        std::memset(b, 0, 16);
        std::memcpy(b, label.c_str(), std::min(label.size(), (size_t)16));
        labelsBuf.insert(labelsBuf.end(), b, b + 16);
        return idx;
    }
};

std::vector<uint8_t> GFF32File::save() {
    if (!m_root) return {};

    WriterState w;

    std::function<uint32_t(const Structure&)> writeStruct;
    writeStruct = [&](const Structure& st) -> uint32_t {
        uint32_t structIdx = w.structCount++;
        size_t structPos = w.structsBuf.size();
        WriterState::append(w.structsBuf, static_cast<uint32_t>(st.structId == -1 ? 0xFFFFFFFF : st.structId));
        WriterState::append(w.structsBuf, uint32_t(0));
        WriterState::append(w.structsBuf, static_cast<uint32_t>(st.fieldOrder.size()));

        std::vector<uint32_t> fieldIndices;
        for (const auto& label : st.fieldOrder) {
            auto it = st.fields.find(label);
            if (it == st.fields.end()) continue;
            const Field& field = it->second;
            uint32_t fieldIdx = w.fieldCount++;
            fieldIndices.push_back(fieldIdx);
            uint32_t labelIdx = w.getLabelIndex(label);
            uint32_t typeId = static_cast<uint32_t>(field.typeId);
            uint32_t dataOrOffset = 0;

            switch (field.typeId) {
                case TypeID::BYTE:
                    dataOrOffset = std::get<uint8_t>(field.value);
                    break;
                case TypeID::CHAR:
                    dataOrOffset = static_cast<uint32_t>(static_cast<uint8_t>(std::get<int8_t>(field.value)));
                    break;
                case TypeID::WORD:
                    dataOrOffset = std::get<uint16_t>(field.value);
                    break;
                case TypeID::SHORT: {
                    int16_t v = std::get<int16_t>(field.value);
                    dataOrOffset = static_cast<uint32_t>(static_cast<uint16_t>(v));
                    break;
                }
                case TypeID::DWORD:
                    dataOrOffset = std::get<uint32_t>(field.value);
                    break;
                case TypeID::INT: {
                    int32_t v = std::get<int32_t>(field.value);
                    std::memcpy(&dataOrOffset, &v, 4);
                    break;
                }
                case TypeID::FLOAT: {
                    float v = std::get<float>(field.value);
                    std::memcpy(&dataOrOffset, &v, 4);
                    break;
                }
                case TypeID::DWORD64: {
                    dataOrOffset = static_cast<uint32_t>(w.fieldDataBuf.size());
                    uint64_t v = std::get<uint64_t>(field.value);
                    WriterState::append(w.fieldDataBuf, v);
                    break;
                }
                case TypeID::INT64: {
                    dataOrOffset = static_cast<uint32_t>(w.fieldDataBuf.size());
                    int64_t v = std::get<int64_t>(field.value);
                    WriterState::append(w.fieldDataBuf, v);
                    break;
                }
                case TypeID::DOUBLE: {
                    dataOrOffset = static_cast<uint32_t>(w.fieldDataBuf.size());
                    double v = std::get<double>(field.value);
                    WriterState::append(w.fieldDataBuf, v);
                    break;
                }
                case TypeID::ExoString: {
                    dataOrOffset = static_cast<uint32_t>(w.fieldDataBuf.size());
                    const auto& s = std::get<std::string>(field.value);
                    uint32_t len = static_cast<uint32_t>(s.size());
                    WriterState::append(w.fieldDataBuf, len);
                    w.fieldDataBuf.insert(w.fieldDataBuf.end(), s.begin(), s.end());
                    break;
                }
                case TypeID::ResRef: {
                    dataOrOffset = static_cast<uint32_t>(w.fieldDataBuf.size());
                    const auto& s = std::get<std::string>(field.value);
                    uint8_t len = static_cast<uint8_t>(std::min(s.size(), (size_t)255));
                    w.fieldDataBuf.push_back(len);
                    w.fieldDataBuf.insert(w.fieldDataBuf.end(), s.begin(), s.begin() + len);
                    break;
                }
                case TypeID::ExoLocString: {
                    dataOrOffset = static_cast<uint32_t>(w.fieldDataBuf.size());
                    const auto& loc = std::get<ExoLocString>(field.value);
                    uint32_t sref = (loc.stringref == -1) ? 0xFFFFFFFF : static_cast<uint32_t>(loc.stringref);
                    uint32_t scount = static_cast<uint32_t>(loc.strings.size());
                    size_t sizePos = w.fieldDataBuf.size();
                    WriterState::append(w.fieldDataBuf, uint32_t(0));
                    WriterState::append(w.fieldDataBuf, sref);
                    WriterState::append(w.fieldDataBuf, scount);
                    for (const auto& ls : loc.strings) {
                        uint32_t sid = (ls.language << 2) | (ls.gender ? 1 : 0);
                        uint32_t slen = static_cast<uint32_t>(ls.text.size());
                        WriterState::append(w.fieldDataBuf, sid);
                        WriterState::append(w.fieldDataBuf, slen);
                        w.fieldDataBuf.insert(w.fieldDataBuf.end(), ls.text.begin(), ls.text.end());
                    }
                    uint32_t totalSize = static_cast<uint32_t>(w.fieldDataBuf.size() - sizePos - 4);
                    std::memcpy(&w.fieldDataBuf[sizePos], &totalSize, 4);
                    break;
                }
                case TypeID::VOID: {
                    dataOrOffset = static_cast<uint32_t>(w.fieldDataBuf.size());
                    const auto& vd = std::get<VoidData>(field.value);
                    uint32_t len = static_cast<uint32_t>(vd.data.size());
                    WriterState::append(w.fieldDataBuf, len);
                    w.fieldDataBuf.insert(w.fieldDataBuf.end(), vd.data.begin(), vd.data.end());
                    break;
                }
                case TypeID::Structure: {
                    auto ptr = std::get<StructurePtr>(field.value);
                    dataOrOffset = writeStruct(ptr ? *ptr : Structure());
                    break;
                }
                case TypeID::List: {
                    auto ptr = std::get<ListPtr>(field.value);
                    dataOrOffset = static_cast<uint32_t>(w.listIndicesBuf.size());
                    uint32_t count = ptr ? static_cast<uint32_t>(ptr->size()) : 0;
                    WriterState::append(w.listIndicesBuf, count);
                    if (ptr) {
                        for (const auto& item : *ptr) {
                            uint32_t childIdx = writeStruct(item);
                            WriterState::append(w.listIndicesBuf, childIdx);
                        }
                    }
                    break;
                }
                default:
                    break;
            }
            WriterState::append(w.fieldsBuf, typeId);
            WriterState::append(w.fieldsBuf, labelIdx);
            WriterState::append(w.fieldsBuf, dataOrOffset);
        }

        if (fieldIndices.size() == 1) {
            std::memcpy(&w.structsBuf[structPos + 4], &fieldIndices[0], 4);
        } else if (fieldIndices.size() > 1) {
            uint32_t indicesOffset = static_cast<uint32_t>(w.fieldIndicesBuf.size());
            std::memcpy(&w.structsBuf[structPos + 4], &indicesOffset, 4);
            for (uint32_t fi : fieldIndices)
                WriterState::append(w.fieldIndicesBuf, fi);
        }
        return structIdx;
    };

    writeStruct(*m_root);

    uint32_t headerSize = 56;
    uint32_t structOff = headerSize;
    uint32_t fieldOff = structOff + static_cast<uint32_t>(w.structsBuf.size());
    uint32_t labelOff = fieldOff + static_cast<uint32_t>(w.fieldsBuf.size());
    uint32_t fieldDataOff = labelOff + static_cast<uint32_t>(w.labelsBuf.size());
    uint32_t fieldIndicesOff = fieldDataOff + static_cast<uint32_t>(w.fieldDataBuf.size());
    uint32_t listIndicesOff = fieldIndicesOff + static_cast<uint32_t>(w.fieldIndicesBuf.size());
    uint32_t totalSize = listIndicesOff + static_cast<uint32_t>(w.listIndicesBuf.size());

    std::vector<uint8_t> out(totalSize);
    std::memcpy(&out[0], m_header.fileType, 4);
    std::memcpy(&out[4], m_header.fileVersion, 4);
    auto w32 = [&](size_t off, uint32_t val) { std::memcpy(&out[off], &val, 4); };
    w32(8, structOff);
    w32(12, w.structCount);
    w32(16, fieldOff);
    w32(20, w.fieldCount);
    w32(24, labelOff);
    w32(28, static_cast<uint32_t>(w.labelMap.size()));
    w32(32, fieldDataOff);
    w32(36, static_cast<uint32_t>(w.fieldDataBuf.size()));
    w32(40, fieldIndicesOff);
    w32(44, static_cast<uint32_t>(w.fieldIndicesBuf.size()));
    w32(48, listIndicesOff);
    w32(52, static_cast<uint32_t>(w.listIndicesBuf.size()));

    std::memcpy(&out[structOff], w.structsBuf.data(), w.structsBuf.size());
    std::memcpy(&out[fieldOff], w.fieldsBuf.data(), w.fieldsBuf.size());
    std::memcpy(&out[labelOff], w.labelsBuf.data(), w.labelsBuf.size());
    if (!w.fieldDataBuf.empty())
        std::memcpy(&out[fieldDataOff], w.fieldDataBuf.data(), w.fieldDataBuf.size());
    if (!w.fieldIndicesBuf.empty())
        std::memcpy(&out[fieldIndicesOff], w.fieldIndicesBuf.data(), w.fieldIndicesBuf.size());
    if (!w.listIndicesBuf.empty())
        std::memcpy(&out[listIndicesOff], w.listIndicesBuf.data(), w.listIndicesBuf.size());

    return out;
}
std::string typeIdToString(TypeID type) {
    switch (type) {
        case TypeID::BYTE: return "BYTE";
        case TypeID::CHAR: return "CHAR";
        case TypeID::WORD: return "WORD";
        case TypeID::SHORT: return "SHORT";
        case TypeID::DWORD: return "DWORD";
        case TypeID::INT: return "INT";
        case TypeID::DWORD64: return "DWORD64";
        case TypeID::INT64: return "INT64";
        case TypeID::FLOAT: return "FLOAT";
        case TypeID::DOUBLE: return "DOUBLE";
        case TypeID::ExoString: return "ExoString";
        case TypeID::ResRef: return "ResRef";
        case TypeID::ExoLocString: return "ExoLocString";
        case TypeID::VOID: return "VOID";
        case TypeID::Structure: return "Structure";
        case TypeID::List: return "List";
        default: return "Unknown";
    }
}
std::string fieldValueToString(const FieldValue& value, TypeID type) {
    std::ostringstream oss;
    switch (type) {
        case TypeID::BYTE:
            oss << static_cast<int>(std::get<uint8_t>(value));
            break;
        case TypeID::CHAR:
            oss << static_cast<int>(std::get<int8_t>(value));
            break;
        case TypeID::WORD:
            oss << std::get<uint16_t>(value);
            break;
        case TypeID::SHORT:
            oss << std::get<int16_t>(value);
            break;
        case TypeID::DWORD:
            oss << std::get<uint32_t>(value);
            break;
        case TypeID::INT:
            oss << std::get<int32_t>(value);
            break;
        case TypeID::DWORD64:
            oss << std::get<uint64_t>(value);
            break;
        case TypeID::INT64:
            oss << std::get<int64_t>(value);
            break;
        case TypeID::FLOAT:
            oss << std::fixed << std::setprecision(6) << std::get<float>(value);
            break;
        case TypeID::DOUBLE:
            oss << std::fixed << std::setprecision(9) << std::get<double>(value);
            break;
        case TypeID::ExoString:
        case TypeID::ResRef:
            oss << "\"" << std::get<std::string>(value) << "\"";
            break;
        case TypeID::ExoLocString:
            oss << std::get<ExoLocString>(value).getDisplayValue();
            break;
        case TypeID::VOID:
            oss << std::get<VoidData>(value).getDisplayValue();
            break;
        case TypeID::Structure: {
            auto ptr = std::get<StructurePtr>(value);
            if (ptr) {
                oss << "(Struct:" << ptr->structId << ", " << ptr->fieldCount() << " fields)";
            } else {
                oss << "(null)";
            }
            break;
        }
        case TypeID::List: {
            auto ptr = std::get<ListPtr>(value);
            if (ptr) {
                oss << "(" << ptr->size() << " items)";
            } else {
                oss << "(empty)";
            }
            break;
        }
        default:
            oss << "???";
            break;
    }
    return oss.str();
}
void walkStructure(const Structure& st, FieldVisitor visitor, const std::string& basePath, int depth) {
    for (const auto& label : st.fieldOrder) {
        auto it = st.fields.find(label);
        if (it == st.fields.end()) continue;
        const Field& field = it->second;
        std::string path = basePath.empty() ? label : basePath + "." + label;
        visitor(path, field, depth);
        if (field.typeId == TypeID::Structure) {
            auto ptr = std::get<StructurePtr>(field.value);
            if (ptr) {
                walkStructure(*ptr, visitor, path, depth + 1);
            }
        } else if (field.typeId == TypeID::List) {
            auto ptr = std::get<ListPtr>(field.value);
            if (ptr) {
                for (size_t i = 0; i < ptr->size(); ++i) {
                    std::string itemPath = path + "[" + std::to_string(i) + "]";
                    Field itemField;
                    itemField.label = std::to_string(i);
                    itemField.typeId = TypeID::Structure;
                    itemField.value = std::make_shared<Structure>((*ptr)[i]);
                    visitor(itemPath, itemField, depth + 1);
                    walkStructure((*ptr)[i], visitor, itemPath, depth + 2);
                }
            }
        }
    }
}
}