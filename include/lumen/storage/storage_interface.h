#ifndef LUMEN_STORAGE_STORAGE_INTERFACE_H
#define LUMEN_STORAGE_STORAGE_INTERFACE_H

#include <lumen/storage/page.h>
#include <lumen/types.h>

#include <memory>

namespace lumen {

// Abstract interface for storage backends
class IStorageBackend {
   public:
    virtual ~IStorageBackend() = default;

    // Page I/O operations
    virtual std::shared_ptr<Page> read_page_from_disk(PageID page_id) = 0;
    virtual bool write_page_to_disk(const Page& page) = 0;
};

}  // namespace lumen

#endif  // LUMEN_STORAGE_STORAGE_INTERFACE_H