#ifndef RAPIDJSON_PRETTYWRITER_H_
#define RAPIDJSON_PRETTYWRITER_H_

#include <rapidjson/writer.h>

#ifdef __GNUC__
RAPIDJSON_DIAG_PUSH
RAPIDJSON_DIAG_OFF(effc++)
#endif

namespace rapidjson {

//! Writer with indentation and spacing.
/*!
    \tparam OutputStream Type of ouptut os.
    \tparam SourceEncoding Encoding of source string.
    \tparam TargetEncoding Encoding of output stream.
    \tparam Allocator Type of allocator for allocating memory of stack.
*/
template<typename OutputStream, typename SourceEncoding = UTF8<>, typename TargetEncoding = UTF8<>, typename Allocator = MemoryPoolAllocator<> >
class PrettyWriter : public Writer<OutputStream, SourceEncoding, TargetEncoding, Allocator> {
public:
    typedef Writer<OutputStream, SourceEncoding, TargetEncoding, Allocator> Base;
    typedef typename Base::Ch Ch;

    //! Constructor
    /*! \param os Output stream.
        \param allocator User supplied allocator. If it is null, it will create a private one.
        \param levelDepth Initial capacity of stack.
    */
    PrettyWriter(OutputStream& os, Allocator* allocator = 0, size_t levelDepth = Base::kDefaultLevelDepth) : 
        Base(os, allocator, levelDepth), indentation_(""), indentCharCount_(0) {}

    //! Overridden for fluent API, see \ref Writer::SetDoublePrecision()
    PrettyWriter& SetDoublePrecision(int p) { Base::SetDoublePrecision(p); return *this; }

    //! Set custom indentation.
    /*! \param indentChar       Character for indentation. Must be whitespace character (' ', '\\t', '\\n', '\\r').
        \param indentCharCount  Number of indent characters for each indentation level.
        \note The default indentation is 4 spaces.
    */
    PrettyWriter& SetIndent(const char* indentation, unsigned indentCharCount) {
        indentation_ = indentation;
        indentCharCount_ = indentCharCount;
        return *this;
    }

    /*! @name Implementation of Handler
        \see Handler
    */
    //@{

    PrettyWriter& Null()                { PrettyPrefix(kNullType);   Base::WriteNull();         return *this; }
    PrettyWriter& Bool(bool b)          { PrettyPrefix(b ? kTrueType : kFalseType); Base::WriteBool(b); return *this; }
    PrettyWriter& Int(int i)            { PrettyPrefix(kNumberType); Base::WriteInt(i);         return *this; }
    PrettyWriter& Uint(unsigned u)      { PrettyPrefix(kNumberType); Base::WriteUint(u);        return *this; }
    PrettyWriter& Int64(int64_t i64)    { PrettyPrefix(kNumberType); Base::WriteInt64(i64);     return *this; }
    PrettyWriter& Uint64(uint64_t u64)  { PrettyPrefix(kNumberType); Base::WriteUint64(u64);    return *this; }
    PrettyWriter& Double(double d)      {
        if (d != d) { Base::os_.Put('N'); Base::os_.Put('a'); Base::os_.Put('N'); } // NaN
        else if (d>0 && d/d != d/d) { Base::os_.Put('I'); Base::os_.Put('n'); Base::os_.Put('f'); Base::os_.Put('i'); Base::os_.Put('n'); Base::os_.Put('i'); Base::os_.Put('t'); Base::os_.Put('y'); } // Infinity
        else if (d<0 && d/d != d/d) { Base::os_.Put('-'); Base::os_.Put('I'); Base::os_.Put('n'); Base::os_.Put('f'); Base::os_.Put('i'); Base::os_.Put('n'); Base::os_.Put('i'); Base::os_.Put('t'); Base::os_.Put('y'); } // -Infinity
        else { PrettyPrefix(kNumberType); Base::WriteDouble(d); }
        return *this;
    }

    PrettyWriter& String(const Ch* str, SizeType length, bool copy = false) {
        (void)copy;
        PrettyPrefix(kStringType);
        Base::WriteString(str, length);
        return *this;
    }

    PrettyWriter& StartObject() {
        PrettyPrefix(kObjectType);
        new (Base::level_stack_.template Push<typename Base::Level>()) typename Base::Level(false);
        Base::WriteStartObject();
        return *this;
    }

    PrettyWriter& EndObject(SizeType memberCount = 0) {
        (void)memberCount;
        RAPIDJSON_ASSERT(Base::level_stack_.GetSize() >= sizeof(typename Base::Level));
        RAPIDJSON_ASSERT(!Base::level_stack_.template Top<typename Base::Level>()->inArray);
        bool empty = Base::level_stack_.template Pop<typename Base::Level>(1)->valueCount == 0;

        if (!empty) {
            if (indentCharCount_ > 0) Base::os_.Put('\n');
            WriteIndent();
        }
        Base::WriteEndObject();
        if (Base::level_stack_.Empty()) // end of json text
            Base::os_.Flush();
        return *this;
    }

    PrettyWriter& StartArray() {
        PrettyPrefix(kArrayType);
        new (Base::level_stack_.template Push<typename Base::Level>()) typename Base::Level(true);
        Base::WriteStartArray();
        return *this;
    }

    PrettyWriter& EndArray(SizeType memberCount = 0) {
        (void)memberCount;
        RAPIDJSON_ASSERT(Base::level_stack_.GetSize() >= sizeof(typename Base::Level));
        RAPIDJSON_ASSERT(Base::level_stack_.template Top<typename Base::Level>()->inArray);
        bool empty = Base::level_stack_.template Pop<typename Base::Level>(1)->valueCount == 0;

        if (!empty) {
            if (indentCharCount_ > 0) Base::os_.Put('\n');
            WriteIndent();
        }
        Base::WriteEndArray();
        if (Base::level_stack_.Empty()) // end of json text
            Base::os_.Flush();
        return *this;
    }

    //@}

    /*! @name Convenience extensions */
    //@{

    //! Simpler but slower overload.
    PrettyWriter& String(const Ch* str) { return String(str, internal::StrLen(str)); }

    //! Overridden for fluent API, see \ref Writer::Double()
    PrettyWriter& Double(double d, int precision) {
        int oldPrecision = Base::GetDoublePrecision();
        return SetDoublePrecision(precision).Double(d).SetDoublePrecision(oldPrecision);
    }

    //@}
protected:
    void PrettyPrefix(Type type) {
        (void)type;
        if (Base::level_stack_.GetSize() != 0) { // this value is not at root
            typename Base::Level* level = Base::level_stack_.template Top<typename Base::Level>();

            if (level->inArray) {
                if (level->valueCount > 0) {
                    Base::os_.Put(','); // add comma if it is not the first element in array
                    if (indentCharCount_ > 0) Base::os_.Put('\n');
                }
                else
                    if (indentCharCount_ > 0) Base::os_.Put('\n');
                WriteIndent();
            }
            else {  // in object
                if (level->valueCount > 0) {
                    if (level->valueCount % 2 == 0) {
                        Base::os_.Put(',');
                        if (indentCharCount_ > 0) Base::os_.Put('\n');
                    }
                    else {
                        Base::os_.Put(':');
                        if (indentCharCount_ > 0) Base::os_.Put(' ');
                    }
                }
                else
                    if (indentCharCount_ > 0) Base::os_.Put('\n');

                if (level->valueCount % 2 == 0)
                    WriteIndent();
            }
            if (!level->inArray && level->valueCount % 2 == 0)
                RAPIDJSON_ASSERT(type == kStringType);  // if it's in object, then even number should be a name
            level->valueCount++;
        }
        else
            // RAPIDJSON_ASSERT(type == kObjectType || type == kArrayType);
            return;
    }

    void WriteIndent()  {
        size_t count = (Base::level_stack_.GetSize() / sizeof(typename Base::Level));
        for (size_t j = 0; j < count; j++)
            for (size_t i = 0; i < indentCharCount_; i++)
                Base::os_.Put(indentation_[i]);
    }

    const char* indentation_;
    unsigned indentCharCount_;

private:
    // Prohibit copy constructor & assignment operator.
    PrettyWriter(const PrettyWriter&);
    PrettyWriter& operator=(const PrettyWriter&);
};

} // namespace rapidjson

#ifdef __GNUC__
RAPIDJSON_DIAG_POP
#endif

#endif // RAPIDJSON_RAPIDJSON_H_
