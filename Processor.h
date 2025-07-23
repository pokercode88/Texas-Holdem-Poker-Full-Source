#ifndef _Processor_H_
#define _Processor_H_

//
#include <util/tc_singleton.h>
#include "GameRecordServant.h"
#include "DataProxyProto.h"
#include "pbtodb.pb.h"
#include "third.pb.h"

//
using namespace tars;

/**
 *请求处理类
 *
 */
class Processor
{
public:
    /**
     *
    */
    Processor();

    /**
     *
    */
    ~Processor();

public:
    //
    int readDataFromDB(long uid, const string& table_name, const std::vector<string>& col_name, const std::vector<vector<string>>& whlist, const string& order_col, dbagent::TDBReadRsp &dataRsp);
    //收藏牌局
    int InsertCollectGame(const gamerecord::CollectGame& collectGame);
    //
    int SelectCollectGame(const long lUid, map<string, vector<gamerecord::CollectGameInfo>>& mapCollectGame);
    //
    int InsertUserGameStat(const long lUid, const string sBriefId, const int gameType, const gamerecord::UserStatInfo& userStatInfo);
    //
    int SelectUserGameStat(const long lUid, const string sBriefId, const int gameType, const long logTime, vector<map<string, string>>& vecGameStat, bool insertFlag = false);
    //
    int InsertGameDetail(const gamerecord::GameDetail& gameDetail);
    //
    int SelectGameDetail(const string sBriefId, const int iRound, vector<map<string, string>>& vecGameDetail);
    //
    int InsertGameBrief(const gamerecord::GameBrief& gameBrief);
    //
    int SelectGameBrief(const string sBriefId, vector<map<string, string>>& vecGameBrief);
    //
    int SelectGameBriefByUid(const long lUid, const int iDays, const int iGameType, vector<map<string, string>>& vecGameBrief);
    //
    int InsertGameDetailCowBoy(const gamerecord::GameDetailCowBoy& gameDetail);
    // 
    int SelectGameDetailCowBoyByUid(const long lUid, const string sBriefId, vector<map<string, string>>& vecGameDetail);
    //
    int InsertGameRecordCowBoy(const gamerecord::GameRecordCowBoy& gameRecord);
    //
    int SelectGameRecordCowBoy(const string sBriefId, vector<map<string, string>>& vecGameRecord);
    //
    int InsertGameDetailCowBoyBanker(const gamerecord::GameDetailCowBoy& gameDetail);
    // 
    int SelectGameDetailCowBoyBankerByUid(const long lUid, const string sBriefId, vector<map<string, string>>& vecGameDetail);
    //
    int InsertGameRecordCowBoyBanker(const gamerecord::GameRecordCowBoyBanker& gameRecord);
    // 
    int SelectGameRecordCowBoyBanker(const string sBriefId, vector<map<string, string>>& vecGameRecord);
    //
    int InsertGameDetailPineApple(const gamerecord::GameDetailPineApple& gameDetail);
    //
    int InsertGameRecordPineApple(const gamerecord::GameRecordPineApple& gameRecord);
    //
    int SelectGameRecordPineApple(const string sBriefId, vector<map<string, string>>& vecGameRecord);
    //
    int SelectGameDetailPineApple(const string sBriefId, const int iRound, vector<map<string, string>>& vecGameDetail);

public:
    bool isAllin(const map<int, gamerecord::UserActStat>& actStat);

    bool isFold(const map<int, gamerecord::UserActStat>& actStat);

    bool isBet(const map<int, gamerecord::UserActStat>& actStat, int iRound = -1);

    string CalRoundBetStat(const string sRoundBetStat, long winGold, bool bWin, const map<int, gamerecord::UserActStat>& actStat);

    string CalCompareStat(const string sCompareStat, long winGold, const map<int, gamerecord::UserActStat>& actStat);

    string CalActStat(const string sAct, const int round, const map<int, gamerecord::UserActStat>& actStat);

    long getStatByRound(const string sStat, const unsigned int iRound);

    long getActStatByAct(const map<string, string>& mapStatInfo, const vector<int>& actList, const vector<string>& roundList);

};

//singleton
typedef TC_Singleton<Processor, CreateStatic, DefaultLifetime> ProcessorSingleton;

#endif

