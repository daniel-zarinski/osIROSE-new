#pragma once


#include "packetfactory.h"
#include "isccommon.h"

namespace RoseCommon {
namespace Packet {

class IscShutdown : public CRosePacket {
    public:
        IscShutdown();
        IscShutdown(CRoseReader reader);
        IscShutdown(IscShutdown&&) = default;
        IscShutdown& operator=(IscShutdown&&) = default;
        ~IscShutdown() = default;
        
        static constexpr size_t size();
        
        
        
        void set_serverType(const Isc::ServerType);
        Isc::ServerType get_serverType() const;
        void set_id(const int32_t);
        int32_t get_id() const;
        
        
        static IscShutdown create(const Isc::ServerType&, const int32_t&);
        static IscShutdown create(const uint8_t*);
        static std::unique_ptr<IscShutdown> allocate(const uint8_t*);
    
    protected:
        virtual void pack(CRoseBasePolicy&) const override;
    
    private:
        Isc::ServerType serverType;
        int32_t id;
};

}
}
