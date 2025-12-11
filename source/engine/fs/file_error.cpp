#include "file_error.h"


const char* FileErrorToString(FileError::Code code) noexcept {
    switch (code) {
    case FileError::Code::None:           return "None";
    case FileError::Code::NotFound:       return "NotFound";
    case FileError::Code::AccessDenied:   return "AccessDenied";
    case FileError::Code::InvalidPath:    return "InvalidPath";
    case FileError::Code::InvalidMount:   return "InvalidMount";
    case FileError::Code::DiskFull:       return "DiskFull";
    case FileError::Code::AlreadyExists:  return "AlreadyExists";
    case FileError::Code::NotEmpty:       return "NotEmpty";
    case FileError::Code::IsDirectory:    return "IsDirectory";
    case FileError::Code::IsNotDirectory: return "IsNotDirectory";
    case FileError::Code::PathTooLong:    return "PathTooLong";
    case FileError::Code::ReadOnly:       return "ReadOnly";
    case FileError::Code::Cancelled:      return "Cancelled";
    case FileError::Code::Unknown:        return "Unknown";
    }
    return "Unknown";
}

std::string FileError::message() const {
    std::string msg = FileErrorToString(code);

    if (!context.empty()) {
        msg += ": ";
        msg += context;
    }

    if (nativeError != 0) {
        msg += " (native error: ";
        msg += std::to_string(nativeError);
        msg += ")";
    }

    return msg;
}

