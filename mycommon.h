#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <deque>
#include <filesystem>
#include <memory>
#include <numeric>
#include <algorithm>
#include <functional>
#include <cassert>
#include <cuchar>
#include <random>

#define rcast reinterpret_cast
#define scast static_cast

#ifndef DebugAssert
#define DebugAssert assert
#endif

namespace fs = std::filesystem;

template <typename T>
using MyArray = std::vector<T>;
template <typename K, typename T>
using MyDict = std::unordered_map<K, T>;
template <typename T>
using MyDeque = std::deque<T>;
using CharString = std::string;
using StringView = std::string_view;
using WideString = std::wstring;
using WStringView = std::wstring_view;
using StringArray = MyArray<CharString>;
using WStringArray = MyArray<WideString>;
using BytesArray = MyArray<uint8_t>;

template <typename T>
using StrongPtr = std::unique_ptr<T>;

template <typename T, typename... Args>
constexpr StrongPtr<T> MakeStrongPtr(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template <typename T>
using RefPtr = std::shared_ptr<T>;

template <typename T, typename... Args>
constexpr RefPtr<T> MakeRefPtr(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}
template <typename T, typename U>
constexpr RefPtr<T> SCastRefPtr(const RefPtr<U>& p) noexcept {
    return std::static_pointer_cast<T>(p);
}



#ifdef __GNUC__
#define PACKED_STRUCT_BEGIN
#define PACKED_STRUCT_END __attribute__((__packed__))
#else
#define PACKED_STRUCT_BEGIN __pragma(pack(push, 1))
#define PACKED_STRUCT_END __pragma(pack(pop))
#endif


class MemStream {
    using OwnedPtrType = std::shared_ptr<uint8_t>;

public:
    MemStream()
        : data(nullptr)
        , length(0)
        , cursor(0) {
    }

    MemStream(const void* _data, const size_t _size, const bool _ownMem = false)
        : data(rcast<const uint8_t*>(_data))
        , length(_size)
        , cursor(0)
    {
        //#NOTE_SK: dirty hack to own a pointer
        if (_ownMem) {
            ownedPtr = OwnedPtrType(const_cast<uint8_t*>(data), free);
        }
    }
    MemStream(const MemStream& other)
        : data(other.data)
        , length(other.length)
        , cursor(other.cursor)
        , ownedPtr(other.ownedPtr) {
    }
    MemStream(MemStream&& other) noexcept
        : data(other.data)
        , length(other.length)
        , cursor(other.cursor)
        , ownedPtr(other.ownedPtr) {
    }
    ~MemStream() {
    }

    inline MemStream& operator =(const MemStream& other) {
        this->data = other.data;
        this->length = other.length;
        this->cursor = other.cursor;
        this->ownedPtr = other.ownedPtr;
        return *this;
    }

    inline MemStream& operator =(MemStream&& other) noexcept {
        this->data = other.data;
        this->length = other.length;
        this->cursor = other.cursor;
        this->ownedPtr.swap(other.ownedPtr);
        return *this;
    }

    inline void SetWindow(const size_t wndOffset, const size_t wndLength) {
        this->cursor = wndOffset;
        this->length = wndOffset + wndLength;
    }

    inline operator bool() const {
        return this->Good();
    }

    inline bool Good() const {
        return (this->data != nullptr && this->length > 0 && !this->Ended());
    }

    inline bool Ended() const {
        return this->cursor >= this->length;
    }

    inline size_t Length() const {
        return this->length;
    }

    inline size_t Remains() const {
        return this->length - this->cursor;
    }

    inline const uint8_t* Data() const {
        return this->data;
    }

    void ReadToBuffer(void* buffer, const size_t bytesToRead) {
        if (this->cursor + bytesToRead <= this->length) {
            std::memcpy(buffer, this->data + this->cursor, bytesToRead);
            this->cursor += bytesToRead;
        }
    }

    void SkipBytes(const size_t bytesToSkip) {
        if (this->cursor + bytesToSkip <= this->length) {
            this->cursor += bytesToSkip;
        } else {
            this->cursor = this->length;
        }
    }

    template <typename T>
    T ReadTyped() {
        T result = T(0);
        if (this->cursor + sizeof(T) <= this->length) {
            result = *rcast<const T*>(this->data + this->cursor);
            this->cursor += sizeof(T);
        }
        return result;
    }

    template <typename T>
    void ReadStruct(T& s) {
        this->ReadToBuffer(&s, sizeof(T));
    }

#define _IMPL_READ_FOR_TYPE(type, name) \
    inline type Read##name() {          \
        return this->ReadTyped<type>(); \
    }

    _IMPL_READ_FOR_TYPE(int8_t, I8);
    _IMPL_READ_FOR_TYPE(uint8_t, U8);
    _IMPL_READ_FOR_TYPE(int16_t, I16);
    _IMPL_READ_FOR_TYPE(uint16_t, U16);
    _IMPL_READ_FOR_TYPE(int32_t, I32);
    _IMPL_READ_FOR_TYPE(uint32_t, U32);
    _IMPL_READ_FOR_TYPE(int64_t, I64);
    _IMPL_READ_FOR_TYPE(uint64_t, U64);
    _IMPL_READ_FOR_TYPE(bool, Bool);
    _IMPL_READ_FOR_TYPE(float, F32);
    _IMPL_READ_FOR_TYPE(double, F64);

#undef _IMPL_READ_FOR_TYPE

    inline size_t GetCursor() const {
        return this->cursor;
    }

    void SetCursor(const size_t pos) {
        this->cursor = std::min<size_t>(pos, this->length);
    }

    inline const uint8_t* GetDataAtCursor() const {
        return this->data + this->cursor;
    }

    MemStream Substream(const size_t subStreamLength) const {
        const size_t allowedLength = ((this->cursor + subStreamLength) > this->Length()) ? (this->Length() - this->cursor) : subStreamLength;
        return MemStream(this->GetDataAtCursor(), allowedLength);
    }

    MemStream Substream(const size_t subStreamOffset, const size_t subStreamLength) const {
        const size_t allowedOffset = (subStreamOffset > this->Length()) ? this->Length() : subStreamOffset;
        const size_t allowedLength = ((allowedOffset + subStreamLength) > this->Length()) ? (this->Length() - allowedOffset) : subStreamLength;
        return MemStream(this->data + allowedOffset, allowedLength);
    }

    MemStream Clone() const {
        if (this->ownedPtr) {
            return *this;
        } else {
            void* dataCopy = malloc(this->Length());
            assert(dataCopy != nullptr);
            memcpy(dataCopy, this->data, this->Length());
            return MemStream(dataCopy, this->Length(), true);
        }
    }

private:
    const uint8_t*  data;
    size_t          length;
    size_t          cursor;
    OwnedPtrType    ownedPtr;
};

template <char a, char b, char c, char d>
constexpr uint32_t MakeFourcc() {
    const uint32_t result = scast<uint32_t>(a) | (scast<uint32_t>(b) << 8) | (scast<uint32_t>(c) << 16) | (scast<uint32_t>(d) << 24);
    return result;
}

template <typename T>
constexpr bool IsPowerOfTwo(const T& x) {
    return (x != 0) && ((x & (x - 1)) == 0);
}
