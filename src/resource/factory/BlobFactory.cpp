#include "resource/factory/BlobFactory.h"
#include "resource/type/Blob.h"
#include "spdlog/spdlog.h"

namespace Ship {
std::shared_ptr<Ship::IResource>
ResourceFactoryBinaryBlobV0::ReadResource(std::shared_ptr<Ship::File> file,
                                          std::shared_ptr<Ship::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto blob = std::make_shared<Blob>(initData);
    auto reader = std::get<std::shared_ptr<Ship::BinaryReader>>(file->Reader);

    uint32_t dataSize = reader->ReadUInt32();

    blob->Data.reserve(dataSize);

    for (uint32_t i = 0; i < dataSize; i++) {
        blob->Data.push_back(reader->ReadUByte());
    }

    return blob;
}
} // namespace Ship
