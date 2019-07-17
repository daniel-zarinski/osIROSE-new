#include "party/party.h"
#include "entity_system.h"
#include "logconsole.h"

#include "components/basic_info.h"
#include "components/client.h"
#include "components/computed_values.h"
#include "components/life.h"
#include "components/party.h"
#include "components/stamina.h"
#include "components/stats.h"

#include "srv_party_reply.h"
#include "srv_party_member.h"
#include "srv_party_req.h"

#include "party_base.h"

using namespace RoseCommon::Packet;

namespace Party {
void add_member(EntitySystem& entitySystem, RoseCommon::Entity leader, RoseCommon::Entity member) {
    auto logger = Core::CLog::GetLogger(Core::log_type::GENERAL).lock();
    logger->trace("Party::add_member");
    auto& p = entitySystem.get_component<Component::Party>(leader);
    const auto& basicInfo_leader = entitySystem.get_component<Component::BasicInfo>(leader);
    const auto& basicInfo_member = entitySystem.get_component<Component::BasicInfo>(member);
    if (entitySystem.has_component<Component::Party>(member)) {
        return;
    }
    auto party = p.party;
    if (!party) {
        party = p.party = std::make_shared<RoseCommon::PartyBase>(basicInfo_leader.tag);
    }
    auto& pp = entitySystem.add_component<Component::Party>(member);
    pp.party = party;
    pp.isKicked = false;
    pp.isRequested = false;
    RoseCommon::PartyData data;
    RoseCommon::PartyData::MemberData m;
    const auto& life = entitySystem.get_component<Component::Life>(member);
    const auto& stats = entitySystem.get_component<Component::Stats>(member);
    const auto& computedValues = entitySystem.get_component<Component::ComputedValues>(member);
    const auto& stamina = entitySystem.get_component<Component::Stamina>(member);
    m.set_tag(basicInfo_member.tag);
    m.set_id(basicInfo_member.id);
    m.set_maxHp(life.maxHp);
    m.set_hp(life.hp);
    m.set_statusFlag(computedValues.statusFlag);
    m.set_con(stats.con);
    m.set_recoveryHp(0);
    m.set_recoveryMp(0);
    m.set_stamina(stamina.stamina);
    m.set_name(basicInfo_member.name);
    data.add_member(m);
    for (const auto& it : party->members) {
        // TODO: check that all entities are on the same map, otherwise send the packet to the correct map for each member
        entitySystem.send_to(entitySystem.get_entity_from_tag(it), SrvPartyMember::create(party->options, data));
    }
    data = RoseCommon::PartyData{};
    for (const auto& i : party->members) {
        const auto it = entitySystem.get_entity_from_tag(i);
        // TODO: check that the entity is on the same map, otherwise send smaller info
        RoseCommon::PartyData::MemberData m;
        const auto& basicInfo = entitySystem.get_component<Component::BasicInfo>(it);
        const auto& life = entitySystem.get_component<Component::Life>(it);
        const auto& stats = entitySystem.get_component<Component::Stats>(it);
        const auto& computedValues = entitySystem.get_component<Component::ComputedValues>(it);
        const auto& stamina = entitySystem.get_component<Component::Stamina>(it);
        m.set_tag(basicInfo.tag);
        m.set_id(basicInfo.id);
        m.set_maxHp(life.maxHp);
        m.set_hp(life.hp);
        m.set_statusFlag(computedValues.statusFlag);
        m.set_con(stats.con);
        m.set_recoveryHp(0);
        m.set_recoveryMp(0);
        m.set_stamina(stamina.stamina);
        m.set_name(basicInfo.name);
        data.add_member(m);
    }
    entitySystem.send_to(member, SrvPartyMember::create(party->options, data));
    party->add_member(basicInfo_member.tag);
}

void remove_member(EntitySystem& entitySystem, RoseCommon::Entity member) {
    auto logger = Core::CLog::GetLogger(Core::log_type::GENERAL).lock();
    logger->trace("Party::remove_member");
    auto& p = entitySystem.get_component<Component::Party>(member);
    if (!p.party) { // no party
        return;
    }
    const auto& basicInfo_member = entitySystem.get_component<Component::BasicInfo>(member);
    auto party = p.party;
    if (!party->remove_member(basicInfo_member.tag)) {
        return;
    }
    const RoseCommon::Entity initiator = p.isKicked ? party->leader : member;
    const uint32_t tag = entitySystem.get_component<Component::BasicInfo>(initiator).tag;
    entitySystem.send_to(member, SrvPartyReply::create(SrvPartyReply::DESTROY, tag));
    if (party->members.size() == 0) {
        return;
    } else if (party->members.size() == 1) {
        // TODO: check if on different map
        entitySystem.remove_component<Component::Party>(entitySystem.get_entity_from_tag(party->members[0]));
        return;
    }
    const uint32_t leaver = entitySystem.get_component<Component::BasicInfo>(member).tag;
    const uint32_t leader = entitySystem.get_component<Component::BasicInfo>(party->leader).tag;
    RoseCommon::PartyData data;
    data.set_tagLeaver(leaver);
    data.set_tagLeader(leader);
    for (const auto& it : party->members) {
        // TODO: check if on same map
        entitySystem.send_to(entitySystem.get_entity_from_tag(it), SrvPartyMember::create(party->options, data));
    }
}

void change_leader(EntitySystem& entitySystem, RoseCommon::Entity current_leader, RoseCommon::Entity new_leader) {
    auto logger = Core::CLog::GetLogger(Core::log_type::GENERAL).lock();
    logger->trace("Party::change_leader");
    auto& p = entitySystem.get_component<Component::Party>(current_leader);
    if (!p.party) { // no party
        entitySystem.remove_component<Component::Party>(current_leader);
        return;
    }
    auto party = p.party;
    if (!party->change_leader(new_leader)) {
        return;
    }
    const uint32_t tag = entitySystem.get_component<Component::BasicInfo>(new_leader).tag;
    for (const auto& it : party->members) {
        entitySystem.send_to(it, SrvPartyReply::create(SrvPartyReply::CHANGE_OWNER, tag));
    }
}

void party_request(EntitySystem& entitySystem, RoseCommon::Entity entity, const CliPartyReq& packet) {
    auto logger = Core::CLog::GetLogger(Core::log_type::GENERAL).lock();
    logger->trace("Party::party_request");
    switch (packet.get_request()) {
        case CliPartyReq::MAKE:
            {
                const RoseCommon::Entity other = entitySystem.get_entity_from_id(packet.get_idXorTag());
                if (other == entt::null || !entitySystem.has_component<Component::Client>(other)) {
                    logger->warn("Client {} requested a party with the non existing char {}", entity, packet.get_idXorTag());
                    return;
                }
                const auto* p = entitySystem.try_get_component<Component::Party>(entity);
                if (p && p->party) {
                    logger->warn("Client {} tried to create a party when already in a party", entity);
                    return;
                }
                const auto& basicInfo = entitySystem.get_component<Component::BasicInfo>(entity);
                auto& pp = entitySystem.add_component<Component::Party>(entity);
                pp.isRequested = true;
                entitySystem.send_to(other, SrvPartyReq::create(static_cast<SrvPartyReq::Request>(packet.get_request()),
                            basicInfo.id, basicInfo.name));
                break;
            }
        case CliPartyReq::JOIN:
            {
                const RoseCommon::Entity other = entitySystem.get_entity_from_id(packet.get_idXorTag());
                if (other == entt::null || !entitySystem.has_component<Component::Client>(other)) {
                    logger->warn("Client {} requested a party with the non existing char {}", entity, packet.get_idXorTag());
                    return;
                }
                auto* p = entitySystem.try_get_component<Component::Party>(entity);
                if (!p || !p->party) {
                    logger->warn("Client {} tried to execute an action on its party but doesn't have one", entity);
                    return;
                }
                const auto& basicInfo = entitySystem.get_component<Component::BasicInfo>(entity);
                p->isRequested = true;
                entitySystem.send_to(other, SrvPartyReq::create(static_cast<SrvPartyReq::Request>(packet.get_request()),
                            basicInfo.id, basicInfo.name));
                break;
            }
        case CliPartyReq::LEFT:
            {
                const RoseCommon::Entity other = entitySystem.get_entity_from_id(packet.get_idXorTag());
                if (other == entt::null || !entitySystem.has_component<Component::Client>(other)) {
                    logger->warn("Client {} requested a party with the non existing char {}", entity, packet.get_idXorTag());
                    return;
                }
                if (!entitySystem.has_component<Component::Party>(entity)) {
                    logger->warn("Client {} tried to leave the party but doesn't have one", entity);
                    return;
                }
                entitySystem.remove_component<Component::Party>(entity);
                break;
            }
        case CliPartyReq::CHANGE_OWNER:
            {
                const RoseCommon::Entity other = entitySystem.get_entity_from_id(packet.get_idXorTag());
                if (other == entt::null || !entitySystem.has_component<Component::Client>(other)) {
                    logger->warn("Client {} requested a party with the non existing char {}", entity, packet.get_idXorTag());
                    return;
                }
                const auto* p = entitySystem.try_get_component<Component::Party>(entity);
                if (!p || !p->party) {
                    logger->warn("Client {} tried to give up ownership but doesn't have a party", entity);
                    return;
                } else if (p->party->leader != entity) {
                    logger->warn("Client {} tried to give up ownership but doesn't have it", entity);
                    return;
                }
                change_leader(entitySystem, entity, other);
                break;
            }
        case CliPartyReq::KICK:
            {
                const RoseCommon::Entity other = entitySystem.get_entity_from_tag(packet.get_idXorTag());
                if (other == entt::null || !entitySystem.has_component<Component::Client>(other)) {
                    logger->warn("Client {} requested a party with the non existing char {}", entity, packet.get_idXorTag());
                    return;
                }
                auto* p = entitySystem.try_get_component<Component::Party>(entity);
                if (!p || !p->party) {
                    logger->warn("Client {} tried to kick a member party but isn't in one", entity);
                    return;
                } else if (!p->party->is_member(other)) {
                    logger->warn("Client {} tried to kick a member that isn't in its party", entity);
                    return;
                }
                entitySystem.get_component<Component::Party>(other).isKicked = true;
                entitySystem.remove_component<Component::Party>(other);
                break;
            }
        default:
            logger->warn("Client {} sent a non valid request code {}", entity, packet.get_request());
    }
}

void party_reply(EntitySystem& entitySystem, RoseCommon::Entity entity, const RoseCommon::Packet::CliPartyReply& packet) {
    auto logger = Core::CLog::GetLogger(Core::log_type::GENERAL).lock();
    logger->trace("Party::party_reply");
    const RoseCommon::Entity other = entitySystem.get_entity_from_id(packet.get_idXorTag());
    if (other == entt::null || !entitySystem.has_component<Component::Client>(other)) {
        logger->warn("Client {} replied to a party request of the non existing char {}", entity, packet.get_idXorTag());
        return;
    }
    auto* p = entitySystem.try_get_component<Component::Party>(other);
    if (!p || !p->isRequested) {
        logger->warn("Client {} replied to a non existing party request", entity);
        return;
    }
    switch (packet.get_reply()) {
        case CliPartyReply::BUSY:
        case CliPartyReply::REJECT:
            p->isRequested = false;
            if (!p->party) {
                entitySystem.remove_component<Component::Party>(other);
            }
            entitySystem.send_to(other, SrvPartyReply::create(static_cast<SrvPartyReply::Reply>(packet.get_reply()),
                        entitySystem.get_component<Component::BasicInfo>(entity).id));
            break;
        case CliPartyReply::ACCEPT_MAKE:
        case CliPartyReply::ACCEPT_JOIN:
            p->isRequested = false;
            entitySystem.send_to(other, SrvPartyReply::create(static_cast<SrvPartyReply::Reply>(packet.get_reply()),
                        entitySystem.get_component<Component::BasicInfo>(entity).id));
            add_member(entitySystem, other, entity);
            break;
        default:
            logger->warn("Client {} sent a party reply with reply {}", entity, (int)packet.get_reply());
    }
}
}
