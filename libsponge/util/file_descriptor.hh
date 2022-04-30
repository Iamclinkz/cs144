#ifndef SPONGE_LIBSPONGE_FILE_DESCRIPTOR_HH
#define SPONGE_LIBSPONGE_FILE_DESCRIPTOR_HH

#include "buffer.hh"

#include <array>
#include <cstddef>
#include <limits>
#include <memory>

//! A reference-counted handle to a file descriptor
//相当于封装了操作系统的文件描述符
class FileDescriptor {
    //! \brief A handle on a kernel file descriptor.
    //! \details FileDescriptor objects contain a std::shared_ptr to a FDWrapper.
    //相当于封装了操作系统的文件描述符的实体类
    class FDWrapper {
      public:
        int _fd;                    //!< The file descriptor number returned by the kernel
        bool _eof = false;          //!< Flag indicating whether FDWrapper::_fd is at EOF
        bool _closed = false;       //!< Flag indicating whether FDWrapper::_fd has been closed
        unsigned _read_count = 0;   //!< The number of times FDWrapper::_fd has been read
        unsigned _write_count = 0;  //!< The numberof times FDWrapper::_fd has been written

        //! Construct from a file descriptor number returned by the kernel
        //explicit 关键字用于表示对于FDWrapper的构造函数的参数,不可以强行转换,比如不可以使用FDWrapper(1)来生成一个FDWrapper对象
        explicit FDWrapper(const int fd);
        //! Closes the file descriptor upon destruction
        ~FDWrapper();
        //! Calls [close(2)](\ref man2::close) on FDWrapper::_fd
        void close();

        //! \name
        //! An FDWrapper cannot be copied or moved

        //!@{
        FDWrapper(const FDWrapper &other) = delete;   //删除了FDWrapper类的默认的拷贝构造等.
        FDWrapper &operator=(const FDWrapper &other) = delete;
        FDWrapper(FDWrapper &&other) = delete;
        FDWrapper &operator=(FDWrapper &&other) = delete;
        //!@}
    };

    //! A reference-counted handle to a shared FDWrapper
    std::shared_ptr<FDWrapper> _internal_fd;    //FileDescriptor类中使用智能指针保存了一个FDWrapper类型的指针

    // private constructor used to duplicate the FileDescriptor (increase the reference count)
    // 只允许使用指向FDWrapper的另一个shared_ptr进行拷贝
    explicit FileDescriptor(std::shared_ptr<FDWrapper> other_shared_ptr);

  protected:
    void register_read() { ++_internal_fd->_read_count; }    //!< increment read count
    void register_write() { ++_internal_fd->_write_count; }  //!< increment write count

  public:
    //! Construct from a file descriptor number returned by the kernel
    //使用传入的fd,创建一个FDWrapper类型的shared_ptr,给内部的_internal_fd赋值
    explicit FileDescriptor(const int fd);

    //! Free the std::shared_ptr; the FDWrapper destructor calls close() when the refcount goes to zero.
    //重写了FDWrapper的析构函数,当refcount为0的之后执行close()
    ~FileDescriptor() = default;

    //! Read up to `limit` bytes
    std::string read(const size_t limit = std::numeric_limits<size_t>::max());

    //! Read up to `limit` bytes into `str` (caller can allocate storage)
    void read(std::string &str, const size_t limit = std::numeric_limits<size_t>::max());

    //! Write a string, possibly blocking until all is written
    //char*类型的重载
    size_t write(const char *str, const bool write_all = true) { return write(BufferViewList(str), write_all); }

    //! Write a string, possibly blocking until all is written
    //string类型的重载
    size_t write(const std::string &str, const bool write_all = true) { return write(BufferViewList(str), write_all); }

    //! Write a buffer (or list of buffers), possibly blocking until all is written
    size_t write(BufferViewList buffer, const bool write_all = true);

    //! Close the underlying file descriptor
    void close() { _internal_fd->close(); }

    //! Copy a FileDescriptor explicitly, increasing the FDWrapper refcount
    //复制一下FileDescriptor,增加其引用计数
    FileDescriptor duplicate() const;

    //! Set blocking(true) or non-blocking(false)
    void set_blocking(const bool blocking_state);

    //! \name FDWrapper accessors
    //!@{
    int fd_num() const { return _internal_fd->_fd; }                         //!< \brief underlying descriptor number
    bool eof() const { return _internal_fd->_eof; }                          //!< \brief EOF flag state
    bool closed() const { return _internal_fd->_closed; }                    //!< \brief closed flag state
    unsigned int read_count() const { return _internal_fd->_read_count; }    //!< \brief number of reads
    unsigned int write_count() const { return _internal_fd->_write_count; }  //!< \brief number of writes
    //!@}

    //! \name Copy/move constructor/assignment operators
    //! FileDescriptor can be moved, but cannot be copied (but see duplicate())
    //!@{
    FileDescriptor(const FileDescriptor &other) = delete;             //!< \brief copy construction is forbidden
    FileDescriptor &operator=(const FileDescriptor &other) = delete;  //!< \brief copy assignment is forbidden
    FileDescriptor(FileDescriptor &&other) = default;                 //!< \brief move construction is allowed
    FileDescriptor &operator=(FileDescriptor &&other) = default;      //!< \brief move assignment is allowed
    //!@}
};

//! \class FileDescriptor
//! In addition, FileDescriptor tracks EOF state and calls to FileDescriptor::read and
//! FileDescriptor::write, which EventLoop uses to detect busy loop conditions.
//!
//! For an example of FileDescriptor use, see the EventLoop class documentation.

#endif  // SPONGE_LIBSPONGE_FILE_DESCRIPTOR_HH
