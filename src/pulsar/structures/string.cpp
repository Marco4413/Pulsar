#include "pulsar/structures/string.h"

Pulsar::String::String()
    : m_Data(nullptr)
    , m_Length(0)
    , m_Capacity(0)
{}

Pulsar::String::~String()
{
    PULSAR_FREE(m_Data);
}

Pulsar::String::String(const String& other)
    : String()
{
    *this = other;
}

Pulsar::String::String(String&& other)
    : String()
{
    *this = std::move(other);
}

Pulsar::String::String(size_t count, char ch)
    : String()
{
    Resize(count);
    PULSAR_MEMSET((void*)m_Data, ch, count*sizeof(char));
}

Pulsar::String::String(const char* data)
    : String(data, std::strlen(data))
{}

Pulsar::String::String(const char* data, size_t length)
    : String()
{
    Resize(length);
    PULSAR_MEMCPY((void*)m_Data, (void*)data, length*sizeof(char));
}

void Pulsar::String::Resize(size_t newLength)
{
    if (!m_Data || newLength > m_Capacity)
        Reserve(newLength*3/2+1);
    m_Length = newLength;
    m_Data[m_Length] = '\0';
}

void Pulsar::String::Reserve(size_t newCapacity)
{
    if (m_Data && newCapacity <= m_Length)
        return;
    m_Data = (char*)PULSAR_REALLOC((void*)m_Data, (newCapacity+1) * sizeof(char));
    m_Capacity = newCapacity;
    m_Data[m_Capacity] = '\0';
}

Pulsar::String Pulsar::String::operator+(const String& other) const
{
    String concat = *this;
    concat += other;
    return concat;
}

Pulsar::String Pulsar::String::operator+(const char* cstr) const
{
    String concat = *this;
    concat += cstr;
    return concat;
}

Pulsar::String Pulsar::String::operator+(char ch) const
{
    String concat = *this;
    concat += ch;
    return concat;
}

bool Pulsar::String::operator==(const String& other) const
{
    if (Length() != other.Length())
        return false;
    for (size_t i = 0; i < other.Length(); i++) {
        if ((*this)[i] != other[i])
            return false;
    }
    return true;
}

Pulsar::String& Pulsar::String::operator+=(const String& other)
{
    size_t appendIdx = m_Length;
    Resize(m_Length+other.m_Length);
    PULSAR_MEMCPY(
        (void*)(m_Data+appendIdx), other.m_Data,
        other.m_Length*sizeof(char));
    return *this;
}

Pulsar::String& Pulsar::String::operator+=(const char* cstr)
{
    size_t cstrLen = std::strlen(cstr);
    size_t appendIdx = m_Length;
    Resize(m_Length+cstrLen);
    PULSAR_MEMCPY(
        (void*)(m_Data+appendIdx), cstr,
        cstrLen*sizeof(char));
    return *this;
}

Pulsar::String& Pulsar::String::operator+=(char ch)
{
    size_t appendIdx = m_Length;
    Resize(m_Length+1);
    m_Data[appendIdx] = ch;
    return *this;
}

Pulsar::String& Pulsar::String::operator=(const String& other)
{
    Reserve(other.m_Length); // Do not allocate more than needed.
    Resize(other.m_Length);
    PULSAR_MEMCPY((void*)m_Data, other.m_Data, other.m_Length*sizeof(char));
    return *this;
}

Pulsar::String& Pulsar::String::operator=(String&& other)
{
    PULSAR_FREE((void*)m_Data);
    m_Data = other.m_Data;
    m_Length = other.m_Length;
    m_Capacity = other.m_Capacity;
    other.m_Data = nullptr;
    other.m_Length = 0;
    other.m_Capacity = 0;
    return *this;
}

int64_t Pulsar::String::Compare(const String& other) const
{
    int64_t cmp = 0;
    if (Length() != other.Length()) {
        size_t minLength = Length() < other.Length()
            ? Length()
            : other.Length();
        for (size_t i = 0; i < minLength; i++) {
            cmp = (*this)[i] - other[i];
            if (cmp != 0)
                break;
        }
        if (cmp == 0)
            cmp = (int64_t)Length() - (int64_t)other.Length();
        return cmp;
    }

    for (size_t i = 0; i < Length(); i++) {
        cmp = (*this)[i] - other[i];
        if (cmp != 0)
            break;
    }
    return cmp;
}

Pulsar::String Pulsar::String::SubString(size_t startIdx, size_t endIdx) const
{
    if (startIdx >= endIdx || startIdx >= m_Length)
        return "";
    else if (endIdx > m_Length)
        return String(&m_Data[startIdx], m_Length-startIdx);
    return String(&m_Data[startIdx], endIdx-startIdx);
}

Pulsar::String Pulsar::UIntToString(uint64_t n)
{
    if (n == 0)
        return "0";
    String res;
    while (n > 0) {
        char digit = '0' + n%10;
        n /= 10;
        res += digit;
    }
    // Reverse string
    size_t endIdx = res.Length();
    for (size_t i = 0; i < res.Length()/2; i++) {
        char ch = res[i];
        res[i] = res[--endIdx];
        res[endIdx] = ch;
    }
    return res;
}

Pulsar::String Pulsar::IntToString(int64_t n)
{
    bool isNegative = n < 0;
    String res = isNegative ? "-" : "";
    res += UIntToString(static_cast<uint64_t>(isNegative ? -n : n));
    return res;
}

Pulsar::String Pulsar::DoubleToString(double n, size_t precision)
{
    if (std::isnan(n))
        return "NaN";
    if (std::isinf(n))
        return n >= 0 ? "+INF" : "-INF";

    bool isNegative = n < 0;
    n = n >= 0 ? n : -n;

    double i, f;
    f = std::modf(n, &i);

    String intPart;
    // Reserve some memory
    intPart.Reserve(16);
    do {
        double id = std::modf(i/10, &i) * 10;
        char digit = '0' + static_cast<int>(id);
        intPart += digit;
    } while (i > 0);
    // Reverse intPart
    for (size_t i = 0, endIdx = intPart.Length(); i < intPart.Length()/2; i++) {
        char ch = intPart[i];
        intPart[i] = intPart[--endIdx];
        intPart[endIdx] = ch;
    }

    String fpPart;
    // Reserve some memory
    fpPart.Reserve(16);
    if (precision > 0) {
        do {
            double fd; f = std::modf(f*10, &fd);
            char digit = '0' + static_cast<int>(fd);
            fpPart += digit;
        } while (f > 0 && fpPart.Length() < precision);
        // Trim all trailing zeroes except 1
        size_t fpLength = fpPart.Length();
        for (; fpLength > 1 && fpPart[fpLength-1] == '0'; --fpLength);
        fpPart.Resize(fpLength);
    } else {
        fpPart = "0";
    }

    String res;
    res.Reserve(intPart.Length()+fpPart.Length() + (isNegative ? 2 : 1) /* Sign and dot */);
    if (isNegative) res += '-';
    res += intPart;
    res += '.';
    res += fpPart;
    return res;
}
