#pragma once

#include "singleton.h"
#include "epackettype.h"
#include "crosepacket.h"
#include <unordered_map>
#include <functional>

namespace RoseCommon {

class PacketFactory : public Singleton<PacketFactory> {
    public:
        template <ePacketType Type, class Class>
        constexpr void registerPacket() {
            mapping_[to_underlying(Type)] = [](uint8_t buffer[MAX_PACKET_SIZE]) -> std::unique_ptr<CRosePacket> {
                return std::make_unique<Class>(buffer);
            };
        }

        std::unique_ptr<CRosePacket> getPacket(uint8_t buffer[MAX_PACKET_SIZE]) {
            try {
                return mapping_.at(to_underlying(CRosePacket::type(buffer)))(buffer);
            } catch (std::out_of_range) {
                return nullptr;
            }
        }

    private:
        using Wrapper = std::function<std::unique_ptr<CRosePacket>(uint8_t[MAX_PACKET_SIZE])>;

        std::unordered_map<std::underlying_type_t<ePacketType>, Wrapper> mapping_;
};

template <ePacketType Type>
struct packet_id {
    static const ePacketType value = Type;
};

template <class Class>
struct packet_type {
    typedef Class type;
};

template <ePacketType Type, class Class>
struct register_id : packet_id<Type>, packet_type<Class> {
    private:
        friend packet_type<Class> packed_id(packet_id<Type>) {
            return packet_type<Class>();
        }
};

template <ePacketType Type, class Class>
class RegisterRecvPacket {
    constexpr static char registerType() {
        PacketFactory::getInstance().registerPacket<Type, Class>();
        return 0;
    }
    static const char __enforce_registration = registerType();

    public:
        virtual ~RegisterRecvPacket() = default;
};

template <ePacketType Type>
struct find_class;

#define REGISTER_SEND_PACKET(Type, Class) template <> struct find_class<Type> : register_id<Type, Class> {};

/*std::unique_ptr<CRosePacket> getPacket(uint8_t buffer[MAX_PACKET_SIZE]) {
    return PacketFactory::getInstance().getPacket(buffer);
}

template <ePacketType T, typename... Args>
std::unique_ptr<decltype(find_class<T>::type)>	makePacket(Args... args) {
    return std::make_unique(args...);
}*/

}
