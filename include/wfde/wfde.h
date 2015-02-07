#pragma once

#include <memory>
#include <chrono>

#include <boost/uuid/uuid.hpp>
#include <boost/filesystem.hpp>
#include <boost/utility/string_ref_fwd.hpp>

#include <wfde/wfde_config.h>
#include <war_basics.h>
#include <war_error_handling.h>
#include <war_asio.h>

#ifndef WFDE_DEFAULT_HOST_LONG_NAME
#   define WFDE_DEFAULT_HOST_LONG_NAME "Jgaa's Fan Club FTP Server"
#endif

namespace war {

class Threadpool;
class Pipeline;

/*! War FTP Daemon Engine
 * \namespace wfde
 *
 * wfde is an acronyom for War FTP Daemon Engine. It is a
 * C++ library providing a basic FTP server implementation
 * meant as a building-block for real FTP server projects,
 * from small embedded services to massively scalable
 * enterprise solutions.
 *
 * This namespace includes the interfaces and factory definitions
 * for this library.
 */

namespace wfde {

class Server;
class Host;
class Protocol;
class Interface;
class Client;
class Session;
class SessionManager;
class AuthManager;

/*! WFDE version */
enum class Version
{
    MAJOR = 0,
    MINOR = 21
};

/*! Program configuration
 *
 * Abstraction for program options in order to be
 * able to use anything from hard-coded values to
 * things like the Apache Storm or databases for
 * configuration
 */
class Configuration
{
public:
    using ptr_t = std::shared_ptr<Configuration>;

    /*! Meta-data about a configuration node */
    struct Node
    {
        /* Name of the node (without any slashes) */
        std::string name;
    };

    /*! Collection of meta-data configuration nodes */
    using node_enum_t = std::vector<Node>;

    Configuration() = default;
    Configuration(const Configuration&) = delete;
    void operator = (const Configuration&) = delete;
    virtual ~Configuration() {};

    /*! Get the sub-configuration for this path and down
     */
    virtual ptr_t GetConfigForPath(std::string path)  = 0;

    /*! Get a value from the configuration.
     *
     * \param path something like
     *     "/Server/Config/Log/filename"
     *
     * \param defaultVal What to retun if the option does not exist.
     *
     * The actual "paths" depends on the options.
     *
     * \returns utf8 string
     */
    virtual std::string GetValue(const char* path,
        const char *defaultVal = nullptr) const = 0;

    /* Check if the program has write-access to the configuration
     * node given as path.
     *
     * \path Path to a node, or nullptr if we just want to check if
     * the repository is read-only or not.
     *
     * \return true if write is allowed. Note that permissions checks
     *  or erroors may case write to fail.
     */
    virtual bool CanWrite(const char* path = nullptr) = 0;

    /*! Set a configuration value.
     *
     * \throws std::exception derived exception on error.
     */
    virtual void SetValue(const char* path, std::string value) = 0;

    /*! Enumerate the nodes at a specific location in the config tree.
     *
     * \return A collection of nodes available at the specified location.
     *      The meta-data specifies if each individual node is a
     *      data-node or a child node.
     */
    virtual node_enum_t EnumNodes(const char *path) = 0;

    /*! Get the configuration
     *
     * \param confFile Configuration file that contains the configuration.
     */
    static ptr_t GetConfiguration(const std::string& confFile);
};


/*! Information about a path.
 *
 *  This class is used to represent nodes on the logical file system
 *  observed by clients, as well as in Client settings and Vhosts
 *  settings to define file system permissions.
 *
 */
class Path
{
public:
    /*! Physical path
     *
     * encoded by the optimal encoding for the operating system / platform.
     *
     *  - On Linux this is an UTF-8 encoded string.
     */
    using ppath_t = boost::filesystem::path;

    /*! Virtual path
     *
     * This is the path as observed from a client. We map
     * physical paths to virtual paths, so that the virtual
     * file system observed by a user normally only represent a
     * small subset of the physical file-system, and often
     * with a totally different layout.
     *
     * The virtual path follows the unix file system path conversion,
     * and starts with a single root-path "/".
     */
    using vpath_t = std::string;

    enum class PermissionBits {
        CAN_READ                = 0b00000000000000000000000000000001,
        CAN_WRITE               = 0b00000000000000000000000000000010,
        CAN_EXECUTE             = 0b00000000000000000000000000000100,
        CAN_ENTER               = 0b00000000000000000000000000001000,
        CAN_LIST                = 0b00000000000000000000000000010000,
        CAN_CREATE_DIR          = 0b00000000000000000000000000100000,
        CAN_CREATE_FILE         = 0b00000000000000000000000001000000,
        CAN_DELETE_FILE         = 0b00000000000000000000000010000000,
        CAN_DELETE_DIR          = 0b00000000000000000000000100000000,
        CAN_SEE_HIDDEN_FILES    = 0b00000000000000000000001000000000,
        CAN_SEE_HIDDEN_DIRS     = 0b00000000000000000000010000000000,
        CAN_CREATE_HIDDEN_FILES = 0b00000000000000000000100000000000,
        CAN_CREATE_HIDDEN_DIRS  = 0b00000000000000000001000000000000,
        CAN_SET_TIMESTAMP       = 0b00000000000000000010000000000000,
        CAN_SET_PERMISSIONS     = 0b00000000000000000100000000000000,
        CAN_RENAME              = 0b00000000000000001000000000000000,
        IS_RECURSIVE            = 0b00000000000000010000000000000000,
        IS_SHARED_UPLOAD_DIR    = 0b00000000000000100000000000000000,
    };

    /*! Type of the path */
    enum class Type { FILE, DIRECTORY, ANY };

    /*! Permission bits as a combined value */
    using permbits_t = uint32_t;

    Path() = default;
    Path& operator = (const Path&) = delete;
    virtual ~Path() {};

    /*! Get the logic path, as observed from the Client */
    virtual const vpath_t& GetVirtualPath() const = 0;

    /*! Get the physical disk-path */
    virtual const ppath_t& GetPhysPath() const = 0;

    /*! Get the "file-name" part of the path.
     *
     * This is the part of the path after the last slash.
     */
    virtual boost::string_ref GetVpathFileName() const = 0;

    // Permissions
    virtual bool CanRead() const noexcept = 0;
    virtual bool CanWrite() const noexcept = 0;
    virtual bool CanExecute() const noexcept = 0;
    virtual bool CanEnter() const noexcept = 0;
    virtual bool CanList() const noexcept = 0;
    virtual bool CanCreateDir() const noexcept = 0;
    virtual bool CanCreateFile() const noexcept = 0;
    virtual bool CanDeleteFile() const noexcept = 0;
    virtual bool CanDeleteDir() const noexcept = 0;
    virtual bool CanSeeHiddenFiles() const noexcept = 0;
    virtual bool CanSeeHiddenDirs() const noexcept = 0;
    virtual bool CanCreateHiddenFile() const noexcept = 0;
    virtual bool CanCreateHiddenDir() const noexcept = 0;
    virtual bool CanSetTimestamp() const noexcept = 0;
    virtual bool CanSetPermissions() const noexcept = 0;
    virtual bool CanRename() const noexcept = 0;

    /// Permissions are applied recursively to subdirs
    virtual bool IsRecursive() const noexcept = 0;

    /*! Is shared upload dir
     *
     * This is a very special location where the session owns the files it
     * has uploaded. When the session is terminated, the files have no owner,
     * and only the public attributes apply.
     *
     * \note Not yet implemented in wfde!
     */
    virtual bool IsSharedUploadDir() const noexcept = 0;

    virtual Type GetType() const = 0;

    /*! Checks if the physical file or directory exists on the file-system */
    virtual bool Exists() const = 0;

    bool IsFile() const { return GetType() == Type::FILE; }

    bool IsDirectory() const { return GetType() == Type::DIRECTORY; }

    virtual bool IsSameParentDir(const Path& path) = 0;

    /*! Return a deep copy of this path */
    virtual std::unique_ptr<Path> Copy() const = 0;

    /*! Return a sub-path from this path

        \param subPath Path to add to the current path. The elements of
            this sub-path is added to both the virtual and physical path
            in the returned object.

        The subPath must be normalized (ie, no ../ tricks)

        This method does not assert or assume anything about the existence
        of the physical path on the actual file-system.
    */
    virtual std::unique_ptr<Path> CreateSubpath(const vpath_t& subPath,
                                                Type pathType) const = 0;

    /*! Convert permissions from a list of enum values to a numeric value */
    static permbits_t ToPermBits(
        const std::initializer_list<PermissionBits>& list) noexcept;

    /*! Convert a comma-sepaarated list of permission tokens to a numeric value */
    static permbits_t ToPermBits(const std::string& list);

    /*! Return a deep copy of this path
     *
     *  but with exactely the specified permissions
     */
    virtual std::unique_ptr<Path> Copy(permbits_t bits) const = 0;

    /*! Split a path into its individual parts.
     *
     * Slashes are removed from the result
     */
    static std::vector<boost::string_ref> Split(const vpath_t& partsToSplit);

    /*! Normalize a path.
     *
     * The returned path will have any "../" and "./" relative indirections
     * removed. The path will also be validated and checked for potential
     * attacks.
     *
     * \exception Throes various war::Exception derived exceptions if illegal
     *      constructs or potential hacker attacks are detected.
     */
    static vpath_t Normalize(const vpath_t& vpath, const vpath_t& currentDir);

    static std::vector<boost::string_ref> NormalizeAndSplit(
        const vpath_t& vpath,
        const vpath_t& currentDir);

    static vpath_t ToVpath(const std::vector<boost::string_ref>& parts,
                           bool addRoot = true);
};


/*! Permissions for a client
 *
 *  Permissions may be granthed at several levels. Before
 *  they are used, all the permission objects relevant for
 *  a Session are merged togehther, and the merged object
 *  is used.
 *
 *  When equal paths are merged, the path at the highest level in the
 *  hirarchy applies. If "/home/jgaa" is defined on the host level as
 *  no access, and again at the user level with read and write access,
 *  the session will have read and write access. Permissions are
 *  not partially merged in such conflicts (like taking the most
 *  strict or most relaxed permissions for each level). The upper
 *  layer will simply redefine the permissions for this path.
 */
class Permissions : public std::enable_shared_from_this<Permissions>
{
public:
    using ptr_t = std::shared_ptr<Permissions>;
    using pathlist_t = std::vector<const Path *>;

    struct BadPathException : public ExceptionBase {};
    struct WrongPathTypeException : public ExceptionBase {};

    /*! Get the best match for the supplied vpath
     *
     * This method is concerned only with permissions. It makes no
     * attempt to verify if the path exists on the file-system.
     *
     * Precondition: path must not end with slash, unless path is
     * the root-path "/". "/home/jgaa" is valid, "/home/jgaa/" is not.
     *
     * Precondition: The math must be normalized.
     */
    virtual const Path& GetPath(const Path::vpath_t& vpath,
                                Path::vpath_t *remaining = nullptr) const = 0;

    virtual void AddPath(std::unique_ptr<Path>&& path) = 0;

    virtual pathlist_t GetPaths() const = 0;

    /*! Merge the permissions with some other permissions.
     *
     * If there are conflicts, the current object's permissions
     * take precidense.
     */
    virtual void Merge(const Permissions& perms) = 0;

    /*! Deep copy */
    virtual ptr_t Copy() const = 0;

};

/*! Base-class for entities within a typichal file-server
 *
 * The entities are non copyable and non copy-constructable.
 *
 * Traditionally, the War FTP Daemon has displayed a three-structure
 * for the server components. This three-structure has proven
 * useful for, and is preserved in wfde.
 *
 * At the basic level, we have a simple structure that lookss like:
 *
 *   Server
 *      +---> Host [Jgaa's Fan Club FTP Server]
 *      |       +---> Protocol [FTP]
 *      |       |        +---> Interface [127.0.0.1:21]
 *      |       |        +---> Interface [192.168.0.100:21]
 *      |       +---> Protocol [HTTP]
 *      |       |        +---> Interface [127.0.0.1:80]
 *      |       |        +---> Interface [10.0.0.100:80]
 *      +---> Host [Private picture collection Server]
 *          ...
 *
 * It is possible to have several Server nodes in a single
 * instance of the wfde process.
 *
 * Users authenticate and create user-sessions on Hosts.
 * One user-session is connected to one protocol. However,
 * a protocol can have many users attached to it.
 *
 * Typically permissions can be defined on all levels in this
 * three, and the users effective permissions is calculated
 * from his server, host and protocol. The interface can have
 * it's own per-session constraints, like IP or host filtering
 * before and after authentication.
 *
 * The Entity interface makes it easy to traverse, configure
 * and query the nodes in a generic way.
 *
 * One may implement sophisticated user and group hirarchies,
 * but at this level it's abstracted away. The hosts and protocol
 * implementations are only concerned about the effective
 * permissions for a user-session.
 */

class Entity
{
public:
    using ptr_t = std::shared_ptr<Entity>;
    using children_t = std::vector<Entity::ptr_t>;

    Entity() = default;
    Entity(const Entity&) = delete;
    Entity& operator = (const Entity&) = delete;

    virtual ~Entity() = default;

    /*! Return the unique name, in the context of the parent entity */
    virtual std::string GetName() const = 0;

    /*! Get the parent entity
     *
     * \return Parent entity or nullptr for the root node.
     *      This is a legacy pointer. A smart pointer
     *      would have been more fashionable, but we
     *      assert that the parent exist at least as
     *      long as the child. So as long as the child
     *      exist, it's safe to access the legacy pointer.
     *      However, do not keep this pointer after letting
     *      go of the instance of this Entity. If you need
     *      a long-living reference, get a shared pointer
     *      or a weak pointer to the parent.
     */
    virtual Entity *GetParent() const = 0;

    /*! Check if this node has a parent */
    virtual bool HaveParent() const = 0;

    /*! Assign the parent node.
     *
     * This method is not thread-save, as it is intended to
     * be used during object initialization.
     *
     * \exception ExceptionAlreadyExist if the parent is already set.
     * \exception std::bad_cast if the parent is of wrong type
     */
    virtual void SetParent(Entity& parent) = 0;

    /*! Get children
     *
     * \param filter Regex to filter the children.
     *
     * \return Child nodes, or an empty list if there are none.
     */
    virtual children_t GetChildren(const std::string& filter = ".*") const = 0;

    virtual Threadpool& GetIoThreadpool() const = 0;

    virtual void SetPermissions(const Permissions::ptr_t& perms) = 0;

    /*! May return nullptr */
    virtual Permissions::ptr_t GetPermissions() const = 0;

    virtual Permissions::ptr_t GetEffectivePermissions() const = 0;
};

/*! Interface for a File Server instance
 */
class Server : virtual public Entity,
    public std::enable_shared_from_this<Server>
{
public:
    using ptr_t = std::shared_ptr<Server>;
    using wptr_t = std::weak_ptr<Server>;
    using hosts_t = std::vector<std::shared_ptr<Host>>;

    /*! Start the server, based on the configuration
     *  given to the factory
     */
    virtual void Start() = 0;

    /*! Stop the server. All listening ports are closed.
     */
    virtual void Stop() = 0;

    /*! Get a list of hosts on the Server.
     * \param filter Regexp on the (short) Hostname.
     */
    virtual hosts_t GetHosts(const std::string& filter = ".*") const = 0;

    /*! Add a host to the server
     *
     * \param host Host to add.
     *
     * \exception ExceptionAlreadyExist if the host-name is alread in use.
     */
    virtual void AddHost(std::shared_ptr<Host> host) = 0;
};


/*! Interface for a Host instance.
 *
 * Think of a host as a HTTP virtual host.
 *
 * A host is basicallly an independent File Server, that can have
 * it's own users, own configuration and apparantly operate independently
 * of other hosts. (Traditional FTP only support a single host on a
 * specific IP/port).
 *
 * In previous versions of War FTP Daemon, this entity was called a "Site"
 *
 * \todo Add virtual hosts on one IP:port for protocols that supports it.
 */
class Host : virtual public Entity,
    public std::enable_shared_from_this<Host>
{
public:
    using ptr_t = std::shared_ptr<Host>;
    using wptr_t = std::weak_ptr<Host>;
    using protocols_t = std::vector<std::shared_ptr<Protocol>>;

    /*! Returns a more verbose name, like
     *      "Jgaa's Fan Club FTP Server"
     *
     * If no long name is defined, it will return the same
     * value as GetName(). Note that unless WFDE_DEFAULT_HOST_LONG_NAME
     * is defined to an empty string, there will always be a default
     * long name.
     */
    virtual std::string GetLongName() const = 0;

    /*! Return the hosts' list of protocols
     */
    virtual protocols_t GetProtocols(const std::string& filter = ".*") const = 0;

    /*! Get a pointer to the server that owns this Host
     */
    virtual Server::ptr_t GetServer() = 0;

    /*! Add a protocol
     *
     * \param protocol Protocol to add
     *
     * \exception ExceptionAlreadyExist if the protocol-name is alread in use.
     */
    virtual void AddProtocol(const std::shared_ptr<Protocol>& protocol) = 0;

    /*! Start the host.
     *
     * Basically, this will open a listening socket on each of the
     * interfaces assigned to all it's protocols.
     */
    virtual void Start() = 0;

    /*! Stop the protocol
     *
     * Stop the host by stopping all it's protocols
     */
    virtual void Stop() = 0;

    /*! Get the session manager associated with the host */
    virtual SessionManager& GetSessionManager() = 0;

    /*! Get the auth-manager for this host */
    virtual AuthManager& GetAuthManager() = 0;
};


/*! Interface for a Protocol
 *
 * The protocol implements a specific Internet protocol, like
 * FTP or HTTP. It deals with the network communication, and
 * obeys the rules defined by it's host.
 */
class Protocol : virtual public Entity,
    public std::enable_shared_from_this<Protocol>
{
public:
    using ptr_t = std::shared_ptr<Protocol>;
    using wptr_t = std::weak_ptr<Protocol>;
    using interfaces_t = std::vector<std::shared_ptr<Interface>>;

    /*! Returns the unique (within the Host) name of the Host

        This will be the name of the protocol implemented,
        like: FTP or HTTP
     */

    /*! Return the protocols' list of interfaces
     */
    virtual interfaces_t GetInterfaces(const std::string& filter = ".*") const = 0;

    /*! Get a pointer to the host that owns this protocol */
    virtual Host& GetHost() = 0;

    /*! Add interfaces as declared in the protocols configuration.
     *
     * This add interfaces based on IP or hostname, and supports
     * IP v4 and IP v6.
     *
     * \return Number of interfaces that were added.
     * \todo Add IPv6 support for the FTP protocol.
     */
    virtual unsigned AddInterfaces() = 0;

    /*! Start the protocol.
     *
     * Basically, this will open a listening socket on each of the
     * interfaces assigned to the protocol.
     */
    virtual void Start() = 0;

    /*! Stop the protocol
     *
     * Closes the listening sockets for this protocol.
     */
    virtual void Stop() = 0;
};

/*! Socket used by a protocol.
 *
 */
class Socket : public std::enable_shared_from_this<Socket>
{
public:
    using write_buffers_t = std::vector<boost::asio::const_buffer>;
    using ptr_t = std::shared_ptr<Socket>;
    using wptr_t = std::weak_ptr<Socket>;

    //operator boost::asio::ip::tcp::socket& () { return GetSocket(); }

    virtual boost::asio::ip::tcp::socket& GetSocket() = 0;
    virtual const boost::asio::ip::tcp::socket& GetSocket() const = 0;
    virtual int GetSocketVal() const = 0;
    virtual Pipeline& GetPipeline() = 0;
    virtual const Pipeline& GetPipeline() const = 0;
    virtual const std::string& GetName() const = 0;

    virtual std::size_t AsyncReadSome(boost::asio::mutable_buffers_1 buffers,
                                      boost::asio::yield_context& yield) = 0;
    virtual std::size_t AsyncRead(boost::asio::mutable_buffers_1 buffers,
                                  boost::asio::yield_context& yield) = 0;
    virtual void AsyncWrite(const boost::asio::const_buffers_1& buffers,
                            boost::asio::yield_context& yield) = 0;
    virtual void AsyncWrite(const write_buffers_t& buffers,
                            boost::asio::yield_context& yield) = 0;
    virtual void AsyncConnect(const boost::asio::ip::tcp::endpoint& ep,
                              boost::asio::yield_context& yield) = 0;

    virtual void AsyncShutdown(boost::asio::yield_context& yield) = 0;

    virtual void Close() = 0;

    virtual bool IsOpen() const = 0;

    virtual boost::asio::ip::tcp::endpoint GetLocalEndpoint() = 0;

    virtual boost::asio::ip::tcp::endpoint GetRemoteEndpoint() = 0;

    virtual void UpgradeToTls(boost::asio::yield_context& yield) = 0;
};

/*! Interface to a protocol
 *
 * This entity represents a listening socket
 * where clients can connect.
 *
 * Typcally it will be a ipv4 or ipv6 endpoint
 * listening for incoming connections.
 *
 * The name is typically the hostname or some abbrivacation
 * configured by the system administrator.
 *
 */
class Interface : virtual public Entity,
    public std::enable_shared_from_this<Interface>
{
public:
    using ptr_t = std::shared_ptr<Interface>;
    using wptr_t = std::weak_ptr<Interface>;
    using handler_t = std::function<void (const Socket::ptr_t& socket)>;

    Interface() = default;

    /*! Get the endpoint assigned to the Interface instance */
    virtual boost::asio::ip::tcp::endpoint GetEndpoint() const = 0;

    /*! Start listening on the interface
     *
     * \param pipeline Pipeline to use for the acceptor
     * \param onConnected Callback to call for incoming calls
     */
    virtual void Start(Pipeline& pipeline, handler_t onConnected) = 0;

    /*! Close the acceptor */
    virtual void Stop() = 0;
};

/*! Abstract representation of a file
 *
 * This interface is optimized for implementations that use
 * memory-mapped files for file-transfers. The idea is to map
 * a sliding "window" of the disk-file directly into the applications
 * address-space, so that we can send and receive parts of the file
 * without performing any memory copying at all - neither in the application
 * nor in the system libraries or by the operating system.
 */
class File
{
public:
    using fpos_t = std::uint64_t;
    using const_buffer_t = boost::asio::const_buffers_1;
    using mutable_buffer_t = boost::asio::mutable_buffers_1;

    enum class FileOperation { READ, WRITE, WRITE_NEW, APPEND };

    File() = default;
    virtual ~File() = default;

    /*! Buffer will be valid until the next read, seek or close
     *
     * Read from the current position and up to bytes bytes.
     *
     * \param bytes Number of bytes to read, or 0 for a reasonable/optimal
     *      default.
     */
    virtual const_buffer_t Read(std::size_t bytes = 0) = 0;

    /*! Buffer will be valid until the next read, seek or close
     *
     * Prepares a write (copy of memory to the buffer) on the current
     * position of the file. The physical file may be enlarged to
     * account to the buffer-space.
     *
     * \param bytes Maximum bytes to write, or 0 for a reasonable/optimal
     *      default.
     */
    virtual mutable_buffer_t Write(std::size_t bytes = 0) = 0;

    /*! Set number of bytes actually written
     *
     * This must always be called after Write, when we know how many bytes
     * that was actually written into the returned memory-buffer.
     *
     * \param bytes Bytes written to in the buffer retuirned by the last
     *      Write method.
     */
    virtual void SetBytesWritten(std::size_t bytes) = 0;

    /*! Get the current file offset */
    virtual fpos_t GetPos() const = 0;

    /*! Get the current size of the file
     *
     * \note When writing, we may pre-allocate disk-space for the
     *      file, so after we have started writing to a file, GetSize() may
     *      report a larger value than what we will observe later, when the
     *      file is closed.
     */
    virtual fpos_t GetSize() const = 0;

    /*! Get the operation this file is for */
    virtual FileOperation GetOperation() const = 0;

    /*! Seek to the given offset from the beginning of the file */
    virtual void Seek(fpos_t pos) = 0;

    /*! Returns true if we are at the end of the file */
    virtual bool IsEof() const = 0;

    /*! Close the file.
     *
     * The File objet can not be re-opened
     */
    virtual void Close() = 0;

    /*! Get the unique ID of this file
     *
     * Each File instance is assigned a unique ID to make it simple to
     * track in log-files.
     */
    virtual const boost::uuids::uuid& GetUuid() const = 0;

    /*! Return the underlaying virtual memory segment size
     * or another reasonable buffre-size.
     */
    virtual std::size_t GetSegmentSize() const noexcept = 0;
};

/*! A logged-in session */
class Session : public std::enable_shared_from_this<Session>
{
public:
    using ptr_t = std::shared_ptr<Session>;
    using wptr_t = std::weak_ptr<Session>;
    using vpath_t = Path::vpath_t;

    struct ExceptionAlreadyLoggedIn : public ExceptionBase {};

    // Abstract representation of data to be owned by the session.
    class Data
    {
        public:
            Data() = default;
            Data(const Data&) = delete;
            Data& operator = (const Data&) = delete;
            virtual ~Data() {}
    };

    /*! Abstract protocol / implementation of specific data
     *
     * Used to start a transfer
     */
    class SessionData : public Data
    {
    public:
        using transfer_func_t = std::function<void (const SessionData&,
            std::unique_ptr<File>)>;

        /*! Start a file transfer */
        virtual void StartTransfer(std::unique_ptr<File>) = 0;
        /* Enable TLS on the protocol */
        virtual void StartTls() = 0;
    };

    Session() = default;
    Session(const Session&) = delete;
    virtual ~Session() = default;
    Session& operator = (const Session&) = delete;

    /*! Try to authenticate a user.
     *
     * Preconditions:
     *      - A user must not be logged on
     *
     * If successful, the authorization properties are updated
     * with thge permissions for this user.
     *
     * \return true on success.
     *
     * \exceptions
     *      ExceptionAlreadyLoggedIn if logged in
     */
    virtual bool AuthenticateWithPasswd(const std::string& name,
                                        const std::string& pwd) = 0;

    /*! Set the effective permissions */
    virtual void SetPermissions(const Permissions::ptr_t& perms) = 0;

    virtual Permissions::ptr_t GetPermissions() = 0;

    /*! Get the current working directoory for this session
     *
     * \returns a "virtual" path to the users current directory.
     */
    virtual vpath_t GetCwd() const = 0;

    /*! Set the current dir for a session */
    virtual void SetCwd(const vpath_t& path) = 0;

    /*! Get a Path instance, corresponding to vpath.
     *
     * This method will use the paths merged into the
     * effective permissions to get the path requested. Vpath can be
     * absolute or relative to the CWD.
     *
     * If the path is invalid, or explicitly forbidden, an exception will
     * be thrown.
     *
     * \param vpath Virtual path, as experienced by the client. This method
     *      can take input given by the client, as validiation and
     *      normalization is performed.
     */
    virtual std::unique_ptr<Path> GetPath(const std::string& vpath,
                                          Path::Type type) = 0;

    /*! Get a path meant for a directory listing
     *
     * Like GetPath(), but in addition it checks the permissions for the
     * intended purpose.
     */
    virtual std::unique_ptr<Path> GetPathForListing(const std::string& vpath) = 0;

    /*! Get a file object to the Path.
     *
     * Path must be a file type.
     *
     * This method checks that the user has the permissions to perform the
     * specified operation on the file.
     */
    virtual std::unique_ptr<File> OpenFile(const vpath_t& path,
                                           File::FileOperation operation) = 0;

    /*! Return a list of virtual paths from this mount-point
     *
     * These are basically mount-points for Path entries below the
     * specified vpath.
     */
    virtual std::vector<boost::string_ref> GetVpaths(const vpath_t& path) = 0;

    /*!  Checks permissions and performs the operatuion */
    virtual void DeleteFile(const vpath_t& path) = 0;

    /*!  Checks permissions and performs the operatuion */
    virtual void CreateDirectory(const vpath_t& path) = 0;

    /*!  Checks permissions and performs the operatuion */
    virtual void DeleteDirectory(const vpath_t& path) = 0;

    /*! Rename a file or directory.
     *
     * The new name must not exist.
     *
     * We allow this to be used as a move operation between directories,
     * provided that the user have the sufficient permissions (CAN_RENAME
     * in the source directory, and CAN_CREATE_FILE/DIR in the target
     * directory. If the operation is a rename in the same directory,
     * only CAN_RENAME permission is required.
     */
    virtual void Rename(const vpath_t& from, const vpath_t& to) = 0;

    virtual File::fpos_t GetFileLen(const vpath_t& path) = 0;

    virtual SessionData& GetSessionData() = 0;

    /*! Get the unique identifier for the session */
    virtual const boost::uuids::uuid& GetUuid() const = 0;

    /*! Get the client for the session */
    virtual std::shared_ptr<Client> GetClient() = 0;

    /*! Set the client for the session */
    virtual void SetClient(std::shared_ptr<Client> client) = 0;

    /*! Get the protocol for this session */
    virtual Protocol& GetProtocol() = 0;

    /*! Get the Socket */
    virtual Socket& GetSocket() = 0;

    /*! Get the host for this session */
    virtual Host& GetHost() = 0;

    virtual boost::asio::ip::tcp::endpoint GetLocalEndpoint() = 0;

    virtual boost::asio::ip::tcp::endpoint GetRemoteEndpoint() = 0;

    /*! Get the time when the session was created */
    virtual std::chrono::system_clock::time_point GetLoginTime() const = 0;

    /*! Get the current idle-time for the session */
    virtual std::chrono::steady_clock::duration GetIdleTime() const = 0;

    /*! Get the time elapsed sincce the session was created. */
    virtual std::chrono::steady_clock::duration GetElapsedTime() const = 0;

    /* Add optional data to be owned by the session */
    virtual void Add(const std::shared_ptr<Data>& data) = 0;

    /*! Set the SessionData object for thiss session
     *
     * This is used by the protocol-implementatioon to route
     * operation(s) partially handled by the session back to itself.
     */
    virtual void Set(SessionData *data) = 0;

    /*! Return the Thrteadpool# thread-id this sessions belongs to */
    virtual int GetThreadId() const noexcept = 0;

    /*! Notify the object that it has been accessed.
     *
     * This updates the idle-time.
     *
     * This method will be called rapidly during transfers.
     * It should therefor return immediately and prefarably not touch
     * any mutexes.
     *
     * \note This method does not need to be implemented in a thread-safe
     *      manner, and must be called only by the owning thread.
     */
    virtual void Touch() = 0;

    /*! Periodically update internal state
     *
     * This method will be called priodically by a timer that iterates over
     * all the sessions it maintains. It should therefore return immediately
     * and prefarably not touch any mutexes.
     *
     * \return true if everything is fine, false if the session should be
     *      closed.
     *
     * \note This method does not need to be implemented in a thread-safe
     *      manner, and must be called only by the owning thread.
     */
    virtual bool OnHousekeeping() = 0;

    /*! Closes the session and cleans up any internally held resources.
     *
     * Note that this does not unregister the session from the session-manager.
     * SessionManager::CloseSession() will call Session::Close(), so there
     * are normally no reasons to call this method directly.
     *
     * \note This method does not need to be implemented in a thread-safe
     *      manner, and must be called only by the owning thread.
     */
    virtual void Close() = 0;
};

/*! Representation of a user or client that is active in a session
 *
 * Note that one client may be logged in to several sessions at the same
 * time.
 */

class Client : public std::enable_shared_from_this<Client>
{
public:
    using ptr_t = std::shared_ptr<Client>;
    using wptr_t = std::weak_ptr<Client>;

    Client() = default;
    virtual ~Client() = default;
    Client(const Client&) = delete;
    Client& operator = (const Client&) = delete;

    /*! Get the login account name for the client
     *
     * The returned reference is only valid while the Client object itself
     * exists.
     */
    virtual const std::string& GetLoginName() const = 0;

    /*! Get the unique identifier for the client
     *
     * The returned reference is only valid while the Client object itself
     * exists.
     */
    virtual const boost::uuids::uuid& GetUuid() const = 0;

    virtual int GetNumInstances() const = 0;

    /*! Get the clients permissioons.
     *
     * May return nullpointer
     */
    virtual Permissions::ptr_t GetPermissions() const = 0;
};

/*! Session manager
 *
 * The manager is responsible for managing sessions.
 */
class SessionManager : public std::enable_shared_from_this<SessionManager>
{
public:
    using ptr_t = std::shared_ptr<SessionManager>;
    using sessions_list_t = std::vector<Session::wptr_t>;

    /*! Parameters for CreateSession */
    struct SessionParams
    {
        Protocol::ptr_t protocol;
        Socket::ptr_t socket;
        Client::ptr_t client; // May be empty;
        uint32_t session_time_out_secs = 60;
    };

    virtual ~SessionManager() {}

    /*! Return a list with all sessions
     */
    virtual sessions_list_t GetSessions() = 0;

    /*! Return a specific session
     *
     * \exception war::ExceptionNotFound If the session is expiered or
     *      not found.
     */
    virtual Session::ptr_t GetSession(const boost::uuids::uuid& id) = 0;

    /*! Create a session.
     *
     * This must be called from the thread that shall own the session,
     * that is - typically the thread for the Socket's pipeline.
     */
    virtual Session::ptr_t CreateSession(const SessionParams& sc) = 0;

    /*! Close session and unregister it from the session manager.
     *
     * \exception war::ExceptionNotFound if the session was not found, or
     *      already expiered.
     *
     * \exception war::ExceptionBase on other errors
     */
    virtual void CloseSession(const boost::uuids::uuid& id) = 0;

    /*! Factory to create a session-manager instance */
    static ptr_t Create(Threadpool& tp);
};

/*! Authentication manager
 *
 * Interface to facilitate user-authentication.
 */
class AuthManager : public std::enable_shared_from_this<AuthManager>
{
public:
    using ptr_t = std::shared_ptr<AuthManager>;

    struct ExceptionNeedPasswd : public ExceptionBase {};
    struct ExceptionBadCredentials : public ExceptionBase {};

    /*! Try to log in a user
     *
     * For FTP, Login() will usually be called twice. First without
     * a password, and then with a password.
     *
     * \return A Client pointer if successful.
     *
     * \exceptions ExceptionNotFound if the user is not found.
     *      ExceptionNeedPasswd if the user require password.
     *      ExceptionBadCredentials if the username/password combination was
     *          wrong.
     */
    virtual Client::ptr_t Login(const std::string& name,
                                const std::string& pwd) = 0;

};


/*! Fabric to create an instance of the Server
 *
 *  Some Configuration settings:
 *      "/Name" : Name of the server. Defaults to "Server".
 *
 *  \param  ioThreadpool Threadpool to use for IO operations.
 *      No long-running tasks will be submitted to this pool
 *      so it is safe to have one thread per core (or less).
 */
Server::ptr_t CreateServer(Configuration::ptr_t conf,
                           Threadpool& ioThreadpool);

/*! Fabric to create an instance of a Host
 *
 *  \param parent Server to join the host to
 *  \param
 *
 *  Some Configuration settings:
 *      "/Name" : Name of the host. Defaults to "Default".
 */
Host::ptr_t CreateHost(Server& parent, AuthManager& authManager,
                       const Configuration::ptr_t& conf);


std::shared_ptr<Permissions> CreatePermissions(Configuration::ptr_t conf);

/*! Fabric to create an instance of a Protocol
 *
 *
 * \param parent If not nullptr, the protocol is assigned to it's parent,
 *      and the parents' AddProtocol() method is invoked on the new
 *      protocol instance. Different protocol-types will have different
 *      configuration properties.
 *
 * \param conf Configuration for the protocol.
 *
 *  Some Configuration settings:
 *      "/Name" : Name of the protocol. This must match the
 *          name of a protocol that is implemented by the server,
 *          like "FTP". The name is case sensitive.
 *
 */
Protocol::ptr_t CreateProtocol(Host *parent, const Configuration::ptr_t conf);

/*! Fabric to create a Interface
 *
 * \param parent who owns this interface
 *
 * \param name Must be unique within the contect of a protocol.
 *      If unset (empty string), a name will be constructed
 *      from the IP address and the port.
 *
 * \param addr IP address to listen to. May be ipv4 or ipv6
 *
 * \param port Port to listen to
 *
 * \param conf Configuration for this interface. May provice extra
 *      options such as ssl options, SOCKS etc.
 *
 * \Note: You can only have one Interface listening to a certan
 *      port on a certan IP address.
 *
 */
Interface::ptr_t CreateInterface(Protocol *parent,
                                 const std::string& name,
                                 const boost::asio::ip::tcp::endpoint& endpoint,
                                 Configuration::ptr_t conf);



/*! Factory type used to create an instance of a protocol */
using protocol_factory_t = std::function<Protocol::ptr_t(Host *parent,
                                                         const Configuration::ptr_t&)>;

/*! Register the factory that creates instances of a specific protocol

    \param name Case sensitive name of the protocol
    \param factory The factory itself
*/
void RegisterProtocolFactory(const std::string& name,
                             protocol_factory_t&& factory);


/*! Register the default WFDE implementations of the available protocols */
void RegisterDefaultProtocols();

} // namespace wfde
} // namerspace war

/* Operators to aid in logging object data */
std::ostream& operator << (std::ostream& o, const war::wfde::Server& entity);
std::ostream& operator << (std::ostream& o, const war::wfde::Protocol& entity);
std::ostream& operator << (std::ostream& o, const war::wfde::Host& entity);
std::ostream& operator << (std::ostream& o, const war::wfde::Interface& entity);
std::ostream& operator << (std::ostream& o, const war::wfde::Socket& sck);
std::ostream& operator << (std::ostream& o, const war::wfde::Version& ver);
std::ostream& operator << (std::ostream& o, const war::wfde::Session& ses);
std::ostream& operator << (std::ostream& o, const war::wfde::Client& ver);
std::ostream& operator << (std::ostream& o, const war::wfde::File& f);
std::ostream& operator << (std::ostream& o, const war::wfde::File::FileOperation& op);
std::ostream& operator << (std::ostream& o, const war::wfde::Permissions& op);

