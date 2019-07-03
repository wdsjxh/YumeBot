#include <string>
#include <vector>
#include <jni.h>

class CCodecWarpper
{
public:
    CCodecWarpper(JavaVM *vm);
    virtual ~CCodecWarpper();

	std::uint32_t CreatePhSigLcIdCheckPacket(std::string&, int, std::vector<std::string>&, const char*);
	int FixAppid();
	void ParseOtherResp(_JNIEnv*, int, CSSOData const&, int);
	void ParsePhSigCheck(_JNIEnv*, CSSOData const&);
	void ParseRecvData(_JNIEnv*);
	void encodeRequest(_JNIEnv*, int, _jstring*, _jstring*, _jstring*, _jstring*, _jstring*,
	                   _jbyteArray*, int, int, _jstring*, signed char, signed char, _jbyteArray*,
	                   uchar);
	jbyteArray getFileStoreKey(_JNIEnv*);

	std::int32_t getMaxPackageSize()
	{
		return m_MaxPackageSize;
	}

private:
    std::uint32_t m_UnknownInt = 0x0000C350;	// 0x04
    std::string m_UnknownStr;					// 0x08
    std::uint32_t m_SignState = 0;				// 0x20
    JavaVM* m_Vm;								// 0x24
	// 指向 com.tencent.qphone.base.util.CodecWarpper
    jclass m_CodecWarpper;						// 0x28
	// 指向 android.content.Context 的实例，当前的 context
    jobject m_Context;							// 0x2C
	// 指向 com.tencent.qphone.base.remote.FromServiceMsg
    jclass m_FromServiceMsg;					// 0x30
    std::vector<int> m_UnknownVec;				// 0x34
    std::int32_t m_MaxPackageSize = 0x00100000;	// 0x40
    std::uint32_t m_UnknownInt2 = 0x21;			// 0x44
    std::string m_Ksid;							// 0x48
};

// sizeof(CCodecWarpper) == 0x60

class CSSOHead
{
public:
    virtual ~CSSOHead();

private:
    std::uint8_t m_UnknownByte = 2;     // 0x04
    std::uint32_t m_KSSOVersion = 8;    // 0x08
    std::string m_D2 = "";      // 0x0C
    std::uint8_t m_UnknownByte2 = 0;    // 0x24
    std::string m_Uin = "";     // 0x28
                                // Uin 即用户的 qq 号
};

// sizeof(CSSOHead) == 0x40

class CSSOReqHead
{
public:
    CSSOReqHead();
    virtual ~CSSOReqHead();

    std::uint32_t Length();
    std::uint32_t Length_ver9();
    bool deSerialize()

private:
    std::int32_t m_SsoSeq;          // 0x04
    std::int32_t m_AppId = -1;    // 0x08
    std::int32_t m_MsfAppId = -1;    // 0x0C
    std::uint8_t m_B2Value = 2;     // 0x10
    std::uint8_t m_UnknownArray[0x0B]{} // 0x11
    std::string m_A2 = "";      // 0x1C
    std::string m_ServiceCmd = "";     // 0x34
    std::string m_Data = "";     // 0x4C, m_Cookie?
    std::string m_ImeiValue = "";     // 0x64
    std::string m_ImsiValue = "";     // 0x7C
    std::string m_TimeStat = "";     // 0x94
    std::string m_Ksid = "";     // 0xAC
    std::int32_t m_UnknownInt4 = 0;     // 0xC4
    std::string m_UnknownStr8 = "";     // 0xC8
    std::int32_t m_UnknownInt = 0;     // 0xE0
};

// sizeof(CSSOReqHead) == 0xE4

class CSSOData
{
public:
    virtual ~CSSOData();

private:
    CSSOHead m_Head;                // 0x0004
    CSSOReqHead m_ReqHead;          // 0x0044
    std::string m_WupBuffer = "";  // 0x0128
    std::uint32_t m_UnknownInt = 0; // 0x0144
};

// sizeof(CSSOData) == 0x0148

class CAuthData
{
public:
    virtual ~CAuthData();

private:
    std::string m_A1;       // 0x04
    std::string m_A2;      // 0x1C
    std::string m_A3;      // 0x34
    std::string m_D1;      // 0x4C
    std::string m_D2;      // 0x64
    std::string m_S2;      // 0x7C
    std::string m_Key;      // 0x94
    std::string m_UnknownData;      // 0xAC
    std::string m_Sid;      // 0xC4
    std::string m_UnknownData2;     // 0xDC
};

// sizeof(CAuthData) == 0xF4

namespace KQQConfig
{
    class SignatureReq
        : public taf::JceStructBase
    {
    public:
    private:
        std::vector<std::string> m_UnknownVecStr;   // 0x00
        std::uint32_t m_AppId;                      // 0x0C
        std::vector<std::string> m_UnknownVecStr2;  // 0x10
        std::uint8_t m_UnknownByte;                 // 0x1C
    };
}

// sizeof(KQQConfig::SignatureReq) == 0x1D?
