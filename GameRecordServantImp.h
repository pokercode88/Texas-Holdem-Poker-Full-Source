#ifndef _GameRecordServantImp_H_
#define _GameRecordServantImp_H_

#include "servant/Application.h"
#include "GameRecordServant.h"
#include "XGameComm.pb.h"
#include "CommonCode.pb.h"
#include "CommonStruct.pb.h"
#include "GameRecord.pb.h"

#include "UserInfoProto.h"

//
using namespace gamerecord;

/**
 *登录服务逻辑处理接口
 *
 */
class GameRecordServantImp : public gamerecord::GameRecordServant
{
public:
    /**
     *
     */
    virtual ~GameRecordServantImp() {}

    /**
     *
     */
    virtual void initialize();

    /**
     *
     */
    virtual void destroy();

public:
    //http请求处理接口
    virtual tars::Int32 doRequest(const vector<tars::Char> &reqBuf, const map<std::string, std::string> &extraInfo, vector<tars::Char> &rspBuf, tars::TarsCurrentPtr current);
    //tcp请求处理接口
    virtual tars::Int32 onRequest(tars::Int64 lUin, const std::string &sMsgPack, const std::string &sCurServrantAddr, const JFGame::TClientParam &stClientParam, const JFGame::UserBaseInfoExt &stUserBaseInfo, tars::TarsCurrentPtr current);
    //获取玩家牌局收藏信息
    virtual tars::Int32 getCollectGame(const CollectGameListReq& req, CollectGameListResp& resp, tars::TarsCurrentPtr current);
    //收藏牌局
    virtual tars::Int32 reportCollectGame(const gamerecord::CollectGame& collectGame, tars::TarsCurrentPtr current);

    virtual tars::Int32 reportGameDetail(const gamerecord::GameDetail& gameDetail, tars::TarsCurrentPtr current);

    virtual tars::Int32 reportGameBrief(const gamerecord::GameBrief& gameBrief, tars::TarsCurrentPtr current);

    virtual tars::Int32 reportGameDetailCowBoy(const gamerecord::GameDetailCowBoy& gameDetail, tars::TarsCurrentPtr current);

    virtual tars::Int32 reportGameRecordCowBoy(const gamerecord::GameRecordCowBoy& gameRecord, tars::TarsCurrentPtr current);

    virtual tars::Int32 reportGameDetailCowBoyBanker(const gamerecord::GameDetailCowBoy& gameDetail, tars::TarsCurrentPtr current);

    virtual tars::Int32 reportGameRecordCowBoyBanker(const gamerecord::GameRecordCowBoyBanker& gameRecord, tars::TarsCurrentPtr current);

    virtual tars::Int32 reportGameDetailPineApple(const gamerecord::GameDetailPineApple& gameRecord, tars::TarsCurrentPtr current);

    virtual tars::Int32 reportGameRecordPineApple(const gamerecord::GameRecordPineApple& gameRecord, tars::TarsCurrentPtr current);

    //获取玩家30天大牌数据
    virtual tars::Int32 getUserGameStat(tars::Int64 lUin, tars::Int32 iGameType, std::string & sData, tars::TarsCurrentPtr current);

public:
    string getUserStat(const long lUid, const string &sBriefId, const int iGameType, const int iDays, bool bVipStat = false);

public:
    int onQueryCollectGameRecord(const XGameComm::TPackage &pkg, const GameRecordProto::QueryCollectGameReq &req, const std::string &sCurServrantAddr);

    //
    int onDeleteCollectGameRecord(const XGameComm::TPackage &pkg, const GameRecordProto::DeleteCollectGameReq &req, const std::string &sCurServrantAddr);

    //
    int onQueryBriefGameRecord(const XGameComm::TPackage &pkg, const GameRecordProto::QueryGameBriefReq &req, const std::string &sCurServrantAddr);
    // 获取战绩(牛仔专用)
    int onQueryBriefGameRecordForCowBoy(const XGameComm::TPackage &pkg, const GameRecordProto::QueryGameBriefReq &req, const std::string &sCurServrantAddr);
    // 获取战绩(大菠萝专用)
    int onQueryBriefGameRecordForPineApple(const XGameComm::TPackage &pkg, const GameRecordProto::QueryGameBriefReq &req, const std::string &sCurServrantAddr);
    // 获取牌谱(牛仔专用)
    int onQueryGameDetailCowBoy(const XGameComm::TPackage &pkg, const GameRecordProto::QueryGameDetailCowBoyReq &req, const std::string &sCurServrantAddr);
    // 获取牌谱(大菠萝专用)
    int onQueryGameDetailPineApple(const XGameComm::TPackage &pkg, const GameRecordProto::QueryGameDetailPineAppleReq &req, const std::string &sCurServrantAddr);
    //
    int onQueryInfoGameRecord(const XGameComm::TPackage &pkg, const GameRecordProto::QueryGameInfoReq &req, const std::string &sCurServrantAddr);
    // 牛仔专用
    int onQueryInfoGameRecordCowBoy(const XGameComm::TPackage &pkg, const GameRecordProto::QueryGameInfoReq &req, const std::string &sCurServrantAddr);
    // 大菠萝专用
    int onQueryInfoGameRecordPineApple(const XGameComm::TPackage &pkg, const GameRecordProto::QueryGameInfoReq &req, const std::string &sCurServrantAddr);
    //
    int onQueryApplyGameRecord(const XGameComm::TPackage &pkg, const GameRecordProto::QueryGameApplyReq &req, const std::string &sCurServrantAddr);
    //
    int onQueryGameDetailRecord(const XGameComm::TPackage &pkg, const GameRecordProto::QueryGameDetailReq &req, const std::string &sCurServrantAddr);
    //
    int onQueryInsureGameRecord(const XGameComm::TPackage &pkg, const GameRecordProto::QueryGameInsureReq &req, const std::string &sCurServrantAddr);

    //
    int onQueryGameStat(const XGameComm::TPackage &pkg, const GameRecordProto::QueryGameStatReq &req, const std::string &sCurServrantAddr);

private:
    //发送消息到客户端
    template<typename T>
    int toClientPb(const XGameComm::TPackage &tPackage, const std::string &sCurServrantAddr, XGameProto::ActionName actionName, const T &t);
};
/////////////////////////////////////////////////////
#endif
