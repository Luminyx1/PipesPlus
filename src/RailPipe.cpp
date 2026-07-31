#include <game_info/CourseInfo.h>
#include <map/CourseData.h>
#include <player/PlayerBase.h>
#include <player/PlayerDemoMgr.h>
#include <player/PlayerModelBaseMgr.h>
#include <red/event/RegisterActorExtensionsEvent.h>

#define TELKIN_REGISTERS
#include <telkin/Telkin.h>

//* Register some extra fields in PlayerBase-class objects

static u32 sPlayerBaseRailPipe_mRailIndex = 0;
static u32 sPlayerBaseRailPipe_mRailTimer = 0;

red::RegisterActorExtensionsEvent::Listener RegisterRailPipeExtensions([](red::RegisterActorExtensionsEvent& e) {
    red::ClassExtension& playerbase = e.extend(PlayerBase::getRuntimeTypeInfoStatic());

    sPlayerBaseRailPipe_mRailIndex = playerbase.add<s16>();
    sPlayerBaseRailPipe_mRailTimer = playerbase.add<u32>();
});

//* Allows us to operate on PlayerBase-class objects with custom extensions
class PlayerBaseRailPipeEx : public PlayerBase {
public:
    void setExitRailDokan();

    [[nodiscard]]
    s16& getRailDokanRailIndex() {
        return *red::ActorClassExtender::get<PlayerBase>(mActorUniqueID)->get<s16>(sPlayerBaseRailPipe_mRailIndex);
    };

    [[nodiscard]]
    s16& getRailDokanNextNodeTimer() {
        return *red::ActorClassExtender::get<PlayerBase>(mActorUniqueID)->get<s16>(sPlayerBaseRailPipe_mRailTimer);
    };

    DECLARE_STATE_ID(PlayerBaseRailPipeEx, DemoRailDokan);
};

//* Make a custom state for the pipe transition "cutscene" and force the game to use it when the connected pipe option is enabled

CREATE_STATE_ID(PlayerBaseRailPipeEx, DemoRailDokan)

void verticalConnectedPipeHook() tAssembly(
    cmpwi r11, 0x2; // replaced instruction
    bnelr;

    // mDokanType (r11) is cDokanType_Rail (0x2)

    tSaveLR;

    // this->changeDemoState(StateID_DemoRailDokan, 0);
    mr r3, r30;
    lis r4, _ZN20PlayerBaseRailPipeEx21StateID_DemoRailDokanE @ha;
    addi r4, r4, _ZN20PlayerBaseRailPipeEx21StateID_DemoRailDokanE @l;
    li r5, 0;
    bl _ZN10PlayerBase15changeDemoStateERK7StateIDi;

    tRestoreLR;

    cmpw r11, r11; // force true branch to return from function
    blr;)
    tBranch(0x28fe1b0, verticalConnectedPipeHook, tk::BranchType::bl);

void horizontalConnectedPipeHook() tAssembly(
    cmpwi r0, 0x2; // replaced instruction
    bnelr;

    // mDokanType (r0) is cDokanType_Rail (0x2)

    tSaveLR;

    // this->changeDemoState(StateID_DemoRailDokan, 0);
    mr r3, r30;
    lis r4, _ZN20PlayerBaseRailPipeEx21StateID_DemoRailDokanE @ha;
    addi r4, r4, _ZN20PlayerBaseRailPipeEx21StateID_DemoRailDokanE @l;
    li r5, 0;
    bl _ZN10PlayerBase15changeDemoStateERK7StateIDi;

    tRestoreLR;

    cmpw r0, r0; // force true branch to return from function
    blr;)
    tBranch(0x028fe638, horizontalConnectedPipeHook, tk::BranchType::bl);

//* Decompiled transition state code from NSMBW: (includes bugfix patches)

void PlayerBaseRailPipeEx::initializeState_DemoRailDokan() {
    onStatus(cStatus_Invisible);

    const CourseDataFile* file = CourseData::instance()->getFile(CourseInfo::instance()->getFileNo());
    const NextGoto* next_goto = file->getNextGoto(mDstNextGotoID);
    const RailInfo* rail = file->getRailInfo(next_goto->rail.info);

    if (next_goto->flag & NextGoto::cFlag_FaceLeft) { // TODO: Use NextGoto::cFlag_ReverseRail
        getRailDokanRailIndex() = rail->point.num - 2;
    } else {
        getRailDokanRailIndex() = 1;
    }

    const RailPoint* node = file->getRailPoint(rail->id) + getRailDokanRailIndex();

    const sead::Vector2f delta(s32(node->offset.x) - mPos.x, -s32(node->offset.y) - mPos.y);

    const f32 dist_len = delta.length();
    getRailDokanNextNodeTimer() = dist_len / 2.0f;
    mDokanPosMoveDelta.set(delta.x / dist_len * 2.0f, delta.y / dist_len * 2.0f);
}

void PlayerBaseRailPipeEx::executeState_DemoRailDokan() {
    if (--getRailDokanNextNodeTimer() < 0) {
        const CourseDataFile* file = CourseData::instance()->getFile(CourseInfo::instance()->getFileNo());
        const NextGoto* next_goto = file->getNextGoto(mDstNextGotoID);
        const RailInfo* rail = file->getRailInfo(next_goto->rail.info);
        const RailPoint* node = file->getRailPoint(rail->id) + getRailDokanRailIndex();

        mPos.x = s32(node->offset.x);
        mPos.y = -s32(node->offset.y);

        bool done = false;
        if (next_goto->flag & NextGoto::cFlag_FaceLeft) {
            getRailDokanRailIndex()--;
            if (getRailDokanRailIndex() < 0) {
                done = true;
            }
        } else {
            getRailDokanRailIndex()++;
            if (getRailDokanRailIndex() >= rail->point.num) {
                done = true;
            }
        }

        if (done) {
            setExitRailDokan();
            return;
        }

        const RailPoint* next_node = file->getRailPoint(rail->id) + getRailDokanRailIndex();

        const sead::Vector2f delta(s32(next_node->offset.x) - mPos.x, -s32(next_node->offset.y) - mPos.y);

        const f32 dist_len = delta.length();
        getRailDokanNextNodeTimer() = dist_len / 2.0f;
        mDokanPosMoveDelta.set(delta.x / dist_len * 2.0f, delta.y / dist_len * 2.0f);
    } else {
        mPos.x += mDokanPosMoveDelta.x;
        mPos.y += mDokanPosMoveDelta.y;
    }
}

void PlayerBaseRailPipeEx::finalizeState_DemoRailDokan() {
    offStatus(cStatus_Invisible);
}

void PlayerBaseRailPipeEx::setExitRailDokan() {
    const CourseDataFile* file = CourseData::instance()->getFile(CourseInfo::instance()->getFileNo());
    const NextGoto* next_goto = file->getNextGoto(mDstNextGotoID);

    next_goto = file->getNextGoto(next_goto->destination.next_goto);

    // mLayer = next_goto->layer; // <- NextGoto doesn't have a layer field in NSMBU... or maybe it does?

    PlayerDemoMgr::instance()->setDemoNo(mPlayerNo);

    switch (next_goto->type) {
        default:
            break;

        case NextGoto::cType_PipeUp:
            changeDemoState(StateID_DemoInDokanD, 1);
            break;

        case NextGoto::cType_PipeDown:
            changeDemoState(StateID_DemoInDokanU, 1);
            break;

        case NextGoto::cType_PipeLeft:
            changeDemoState(StateID_DemoInDokanR, 1);
            break;

        case NextGoto::cType_PipeRight:
            changeDemoState(StateID_DemoInDokanL, 1);
            break;
    }
}
