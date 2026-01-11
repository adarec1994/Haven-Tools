#pragma once
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <cstdint>
#include <memory>
#include <functional>
namespace GFF32 {
struct Structure;
struct List;
struct ExoLocString;
enum class TypeID : uint32_t {
    BYTE = 0,
    CHAR = 1,
    WORD = 2,
    SHORT = 3,
    DWORD = 4,
    INT = 5,
    DWORD64 = 6,
    INT64 = 7,
    FLOAT = 8,
    DOUBLE = 9,
    ExoString = 10,
    ResRef = 11,
    ExoLocString = 12,
    VOID = 13,
    Structure = 14,
    List = 15
};
struct LocalString {
    uint32_t language;
    bool gender;  
    std::string text;
};
struct ExoLocString {
    int32_t stringref;
    std::vector<LocalString> strings;
    ExoLocString() : stringref(-1) {}
    std::string getDisplayValue() const;
};
struct VoidData {
    std::vector<uint8_t> data;
    std::string getDisplayValue() const;
};
struct Structure;
using ListPtr = std::shared_ptr<std::vector<Structure>>;
using StructurePtr = std::shared_ptr<Structure>;
using FieldValue = std::variant<
    uint8_t,        
    int8_t,         
    uint16_t,       
    int16_t,        
    uint32_t,       
    int32_t,        
    uint64_t,       
    int64_t,        
    float,          
    double,         
    std::string,    
    ExoLocString,   
    VoidData,       
    StructurePtr,   
    ListPtr         
>;
struct Field {
    std::string label;
    TypeID typeId;
    FieldValue value;
    std::string getTypeName() const;
    std::string getDisplayValue() const;
    bool isComplex() const;
};
struct Structure {
    int32_t structId;
    std::vector<std::string> fieldOrder;  
    std::map<std::string, Field> fields;
    std::string fileType;
    std::string fileVersion;
    Structure() : structId(-1) {}
    bool hasField(const std::string& label) const;
    const Field* getField(const std::string& label) const;
    Field* getField(const std::string& label);
    void setField(const std::string& label, TypeID type, FieldValue value);
    size_t fieldCount() const { return fields.size(); }
    std::vector<std::string>::const_iterator begin() const { return fieldOrder.begin(); }
    std::vector<std::string>::const_iterator end() const { return fieldOrder.end(); }
};
struct Header {
    char fileType[5];      
    char fileVersion[5];   
    uint32_t structOffset;
    uint32_t structCount;
    uint32_t fieldOffset;
    uint32_t fieldCount;
    uint32_t labelOffset;
    uint32_t labelCount;
    uint32_t fieldDataOffset;
    uint32_t fieldDataCount;
    uint32_t fieldIndicesOffset;
    uint32_t fieldIndicesCount;
    uint32_t listIndicesOffset;
    uint32_t listIndicesCount;
};
class GFF32File {
public:
    GFF32File();
    ~GFF32File();
    bool load(const std::string& path);
    bool load(const std::vector<uint8_t>& data);
    bool load(const uint8_t* data, size_t size);
    bool save(const std::string& path);
    std::vector<uint8_t> save();
    void close();
    bool isLoaded() const { return m_loaded; }
    const Header& header() const { return m_header; }
    Structure* root() { return m_root.get(); }
    const Structure* root() const { return m_root.get(); }
    std::string fileType() const;
    std::string fileVersion() const;
    bool is2DA() const;
    bool isDLG() const;
    bool isUTI() const;
    bool isUTC() const;
    bool isUTP() const;
    static bool isGFF32(const std::vector<uint8_t>& data);
    static bool isGFF32(const uint8_t* data, size_t size);
private:
    bool parseHeader(const uint8_t* data, size_t size);
    bool parseContent(const uint8_t* data, size_t size);
    Structure parseStruct(const uint8_t* data, size_t size, 
                          const std::vector<std::string>& labels,
                          uint32_t structIndex,
                          std::vector<Structure>& structs);
    template<typename T>
    T readAt(const uint8_t* data, size_t offset) const {
        T val;
        std::memcpy(&val, data + offset, sizeof(T));
        return val;
    }
    Header m_header;
    std::shared_ptr<Structure> m_root;
    bool m_loaded;
};
std::string typeIdToString(TypeID type);
std::string fieldValueToString(const FieldValue& value, TypeID type);
using FieldVisitor = std::function<void(const std::string& path, const Field& field, int depth)>;
void walkStructure(const Structure& st, FieldVisitor visitor, const std::string& basePath = "", int depth = 0);
}