
#ifndef CLOUDRENDER_FILEUTIL_H
#define CLOUDRENDER_FILEUTIL_H
#include "global/baseDef.h"

    class FileUtil {
    public:
        static std::vector<char> readBinary(const std::string& fileName);
        static void writeBinary(char* data, uint32_t size, char* name);
        static char* getFileName(std::string filePath);
        static bool isStringPrintable(const std::string& str);
        static std::string utf8ToANIS(const std::string utf8_str); //中文Windows系统下ANIS编码即GBK编码
        static std::string ANISToUtf8(const std::string utf8_str);

        static std::string concatURL(const std::string ip, int port, const std::string interfaceStr);
        static std::string concatFullFilePath(const std::string fileName, const std::string filePath);
    };



#endif //CLOUDRENDER_FILEUTIL_H
