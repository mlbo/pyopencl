#include "wrap_cl.h"
#include <string>
#include <sstream>
#include <string.h>
#include <memory>

#define PYOPENCL_DEF_GET_CLASS_T(name)          \
    static inline class_t                       \
    get_class_t()                               \
    {                                           \
        return CLASS_##name;                    \
    }


template<class T>
static inline std::string
tostring(const T& v)
{
    std::ostringstream ostr;
    ostr << v;
    return ostr.str();
}


template<typename T>
struct _D {
    void operator()(T *p) {
        free((void*)p);
    }
};

template<typename T>
class pyopencl_buf : public std::unique_ptr<T, _D<T> > {
    size_t m_len;
public:
    pyopencl_buf(size_t len=1) :
        std::unique_ptr<T, _D<T> >((T*)(len ? malloc(sizeof(T) * len) : NULL)),
        m_len(len)
    {
    }
    inline size_t
    len()
    {
        return m_len;
    }
    inline T&
    operator[](int i)
    {
        return this->get()[i];
    }
    inline void
    resize(size_t len)
    {
        if (len == m_len)
            return;
        m_len = len;
        this->reset((T*)realloc((void*)this->release(), len * sizeof(T)));
    }
};

template<>
class pyopencl_buf<void> : public std::unique_ptr<void, _D<void> > {
    size_t m_len;
public:
    pyopencl_buf(size_t len) :
        std::unique_ptr<void, _D<void> >((len ? malloc(len) : NULL)),
        m_len(len)
    {
    }
    inline size_t
    len()
    {
        return m_len;
    }
    inline void
    resize(size_t len)
    {
        if (len == m_len)
            return;
        m_len = len;
        this->reset(realloc(this->release(), len));
    }
};

namespace pyopencl {

// FIXME
static inline char*
_copy_str(const std::string& str)
{
    return strdup(str.c_str());
}

template<typename T, typename... ArgTypes, typename... ArgTypes2>
static inline pyopencl_buf<T>
get_vec_info(cl_int (*func)(ArgTypes...), const char *name,
             ArgTypes2&&... args)
{
    size_t size = 0;
    call_guarded(func, name, args..., 0, NULL, &size);
    pyopencl_buf<T> buf(size / sizeof(T));
    call_guarded(func, name, args..., size, buf.get(), &size);
    return buf;
}
#define pyopencl_get_vec_info(type, what, args...)                      \
    pyopencl::get_vec_info<type>(clGet##what##Info, "clGet" #what "Info", args)

template<typename T>
static inline generic_info
convert_array_info(const char *tname, pyopencl_buf<T> &buf)
{
    generic_info info;
    info.dontfree = 0;
    info.opaque_class = CLASS_NONE;
    info.type = _copy_str(std::string(tname) + "[" +
                          tostring(buf.len()) + "]");
    info.value = buf.release();
    return info;
}

template<typename T>
static inline generic_info
convert_array_info(const char *tname, pyopencl_buf<T> &&_buf)
{
    pyopencl_buf<T> &buf = _buf;
    return convert_array_info<T>(tname, buf);
}

#define pyopencl_convert_array_info(type, buf)          \
    pyopencl::convert_array_info<type>(#type, buf)
#define pyopencl_get_array_info(type, what, args...)                    \
    pyopencl_convert_array_info(type, pyopencl_get_vec_info(type, what, args))

template<typename T, typename Cls>
static inline generic_info
convert_opaque_array_info(pyopencl_buf<T> &buf)
{
    pyopencl_buf<void*> ar(buf.len());
    for (unsigned i = 0;i < buf.len();i++) {
        ar[i] = new Cls(buf[i]);
    }
    generic_info info;
    info.dontfree = 0;
    info.opaque_class = Cls::get_class_t();
    info.type = _copy_str(std::string("void*[") + tostring(buf.len()) + "]");
    info.value = ar.release();
    return info;
}

template<typename T, typename Cls>
static inline generic_info
convert_opaque_array_info(pyopencl_buf<T> &&_buf)
{
    pyopencl_buf<T> &buf = _buf;
    return convert_opaque_array_info<T, Cls>(buf);
}
#define pyopencl_get_opaque_array_info(type, cls, what, args...)  \
    pyopencl::convert_opaque_array_info<type, cls>(              \
        pyopencl_get_vec_info(type, what, args))

}
