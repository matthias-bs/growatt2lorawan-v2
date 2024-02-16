#define SECRETS

//-------------------------------
// --> REPLACE BY YOUR VALUES <--
//-------------------------------

// application identifier - pre-LoRaWAN 1.1.0, this was called appEUI
// when adding new end device in TTN, you will have to enter this number
// you can pick any number you want, but it has to be unique
uint64_t joinEUI = 0x12AD1011B0C0FFEE;

// device identifier - this number can be anything
// when adding new end device in TTN, you can generate this number,
// or you can set any value you want, provided it is also unique
uint64_t devEUI = 0x70B3D57ED005E120;

// select some encryption keys which will be used to secure the communication
// there are two of them - network key and application key
// because LoRaWAN uses AES-128, the key MUST be 16 bytes (or characters) long

// network key is the ASCII string "topSecretKey1234"
uint8_t nwkKey[] = { 0x74, 0x6F, 0x70, 0x53, 0x65, 0x63, 0x72, 0x65,
                     0x74, 0x4B, 0x65, 0x79, 0x31, 0x32, 0x33, 0x34 };

// application key is the ASCII string "aDifferentKeyABC"
uint8_t appKey[] = { 0x61, 0x44, 0x69, 0x66, 0x66, 0x65, 0x72, 0x65,
                     0x6E, 0x74, 0x4B, 0x65, 0x79, 0x41, 0x42, 0x43 };

// prior to LoRaWAN 1.1.0, only a single "nwkKey" is used
// when connecting to LoRaWAN 1.0 network, "appKey" will be disregarded
// and can be set to NULL