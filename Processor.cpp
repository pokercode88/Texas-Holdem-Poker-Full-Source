#include "Processor.h"
#include "globe.h"
#include "LogComm.h"
#include "DataProxyProto.h"
#include "ServiceDefine.h"
#include "util/tc_hash_fun.h"
#include "uuid.h"
#include "GameRecordServer.h"
#include "util/tc_base64.h"

//
using namespace std;
using namespace dataproxy;
using namespace dbagent;

#define ZEROTIME (TNOW - 8 * 3600) - (TNOW - 8 * 3600) % 86400

/**
 *
*/
Processor::Processor()
{
}

/**
 *
*/
Processor::~Processor()
{
}

#define __NASH_RELEASE__
#ifdef __NASH_RELEASE__
static const bool DEBUG_VERSION = false;
#else
static const bool DEBUG_VERSION = true;
#endif // __NASH_RELEASE__

#define __CATCH_OUT_OF_RANGE__ }\
    catch (std::out_of_range& e)\
    {\
            ostringstream os;   \
            os << "[OUT_OF_RANGE]:"<<__FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ ;   \
            throw logic_error(os.str());    \
    }

int Processor::readDataFromDB(long uid, const string& table_name, const std::vector<string>& col_name, const std::vector<vector<string>>& whlist, const string& order_col, dbagent::TDBReadRsp &dataRsp)
{
    int iRet = 0;
    dbagent::TDBReadReq rDataReq;
    rDataReq.keyIndex = 0;
    rDataReq.queryType = dbagent::E_SELECT;
    rDataReq.tableName = table_name;

    vector<dbagent::TField> fields;
    dbagent::TField tfield;
    tfield.colArithType = E_NONE;
    for(auto item : col_name)
    {
        tfield.colName = item;
        fields.push_back(tfield);
    }
    rDataReq.fields = fields;

    //where条件组
    if(!whlist.empty())
    {
        vector<dbagent::ConditionGroup> conditionGroups;
        dbagent::ConditionGroup conditionGroup;
        conditionGroup.relation = dbagent::AND;
        vector<dbagent::Condition> conditions;
        for(auto item : whlist)
        {
            if(item.size() != 3)
            {
                continue;
            }
            dbagent::Condition condition;
            condition.condtion =  dbagent::Eum_Condition(S2I(item[1]));
            condition.colType = dbagent::STRING;
            condition.colName = item[0];
            condition.colValues = item[2];
            conditions.push_back(condition);
        }
        conditionGroup.condition = conditions;
        conditionGroups.push_back(conditionGroup);
        rDataReq.conditions = conditionGroups;
    }

    //order by字段
    if(!order_col.empty())
    {
        vector<dbagent::OrderBy> orderBys;
        dbagent::OrderBy orderBy;
        orderBy.sort = dbagent::DESC;
        orderBy.colName = order_col;
        orderBys.push_back(orderBy);
        rDataReq.orderbyCol = orderBys;
    }
    iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(uid)->read(rDataReq, dataRsp);
    if (iRet != 0 || dataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "read data from dbagent failed, rDataReq:" << printTars(rDataReq) << ",dataRsp: " << printTars(dataRsp) << endl;
        return -1;
    }
    return 0;
}

int Processor::InsertCollectGame(const gamerecord::CollectGame& collectGame)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    ROLLLOG_DEBUG << "collectGame: "<< printTars(collectGame) << endl;
    if(collectGame.lPlayerID <= 0 || collectGame.sBriefId.empty() || collectGame.iRound < 0)
    {
        return -1;
    }

    dataproxy::TWriteDataReq dataReq;
    dataReq.resetDefautlt();
    dataReq.keyName = I2S(E_REDIS_TYPE_LIST) + ":" + I2S(GAME_RECORD_COLLECT) + ":" + L2S(collectGame.lPlayerID);
    dataReq.operateType = E_REDIS_WRITE;
    dataReq.clusterInfo.resetDefautlt();
    dataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    dataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_USER_ID;
    dataReq.clusterInfo.frageFactor = collectGame.lPlayerID;
    dataReq.paraExt.resetDefautlt();
    dataReq.paraExt.queryType = E_REPLACE;

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = E_NONE;
    tfield.colName = "uid";
    tfield.colType = BIGINT;
    tfield.colValue = L2S(collectGame.lPlayerID);
    fields.push_back(tfield);
    tfield.colName = "brief_id";
    tfield.colType = STRING;
    tfield.colValue = collectGame.sBriefId;
    fields.push_back(tfield);
    tfield.colName = "round";
    tfield.colType = INT;
    tfield.colValue = I2S(collectGame.iRound);
    fields.push_back(tfield);
    tfield.colName = "state";
    tfield.colType = INT;
    tfield.colValue = L2S(collectGame.iOption);
    fields.push_back(tfield);
    tfield.colName = "game_type";
    tfield.colType = INT;
    tfield.colValue = L2S(collectGame.iGameType);
    fields.push_back(tfield);
    dataReq.fields = fields;

    dataproxy::TWriteDataRsp dataRsp;
    iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(collectGame.lPlayerID)->redisWrite(dataReq, dataRsp);
    if (iRet != 0 || dataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "collect game err, iRet: " << iRet << ", iResult: " << dataRsp.iResult << endl;
        return -2;
    }

    ROLLLOG_DEBUG << "collect game, iRet: " << iRet << ", dataRsp: " << printTars(dataRsp) << endl;

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

int Processor::SelectCollectGame(const long lUid, map<string, vector<gamerecord::CollectGameInfo>>& mapCollectGame)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    dataproxy::TReadDataReq dataReq;
    dataReq.resetDefautlt();
    dataReq.keyName = I2S(E_REDIS_TYPE_LIST) + ":" + I2S(GAME_RECORD_COLLECT) + ":" + L2S(lUid);
    dataReq.operateType = E_REDIS_READ;
    dataReq.clusterInfo.resetDefautlt();
    dataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    dataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_USER_ID;
    dataReq.clusterInfo.frageFactor = lUid;
    dataReq.paraExt.resetDefautlt();
    dataReq.paraExt.subOperateType = E_REDIS_LIST_RANGE;//根据范围取数据
    dataReq.paraExt.start = 0;//起始下标从0开始
    dataReq.paraExt.end = -1;//终止最大结束下标为-1

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = E_NONE;
    tfield.colName = "brief_id";
    fields.push_back(tfield);
    tfield.colName = "round";
    fields.push_back(tfield);
    tfield.colName = "game_type";
    fields.push_back(tfield);
    dataReq.fields = fields;

    auto &conditionFields = dataReq.paraExt.fields;
    tfield.resetDefautlt();
    tfield.colValue = "1";
    conditionFields["state"] = tfield;

    dataproxy::TReadDataRsp dataRsp;
    iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(lUid)->redisRead(dataReq, dataRsp);
    if (iRet != 0 || dataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "get collect game err, iRet: " << iRet << ", iResult: " << dataRsp.iResult << endl;
        return -1;
    }

    gamerecord::CollectGameInfo info;
    for (auto it = dataRsp.fields.begin(); it != dataRsp.fields.end(); ++it)
    {
        for (auto itTField = it->begin(); itTField != it->end(); ++itTField)
        {
            if (itTField->colName == "brief_id")
            {
                info.sBriefId = itTField->colValue;
            }
            else if (itTField->colName == "round")
            {
                info.iRound = S2I(itTField->colValue);
            }
            else if (itTField->colName == "game_type")
            {
                info.iGameType = S2I(itTField->colValue);
            }
        }
        auto iter = mapCollectGame.find(info.sBriefId);
        if(iter == mapCollectGame.end())
        {
            mapCollectGame.insert(std::make_pair(info.sBriefId, vector<gamerecord::CollectGameInfo>(1, info)));
        }
        else
        {
            iter->second.push_back(info);
        }
    }

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

bool Processor::isAllin(const map<int, gamerecord::UserActStat>& actStat)
{
    for(auto item : actStat)
    {
        auto it = item.second.mapActCount.find(256);
        if(it != item.second.mapActCount.end())
        {
            return true;
        }
    }
    return false;
}

bool Processor::isFold(const map<int, gamerecord::UserActStat>& actStat)
{
    for(auto item : actStat)
    {
        auto it = item.second.mapActCount.find(16);
        if(it != item.second.mapActCount.end())
        {
            return true;
        }
    }
    return false;
}

bool Processor::isBet(const map<int, gamerecord::UserActStat>& actStat, int iRound)
{
    for(auto item : actStat)
    {
        auto it1 = item.second.mapActCount.find(256);
        auto it2= item.second.mapActCount.find(128);
        auto it3 = item.second.mapActCount.find(64);
        if((iRound >= 0 && item.first == iRound) || iRound < 0)
        {
            return it1 != item.second.mapActCount.end() || it2 != item.second.mapActCount.end() || it3 != item.second.mapActCount.end();
        }
    }
    return false;
}

string Processor::CalRoundBetStat(const string sRoundBetStat, long winGold, bool bWin, const map<int, gamerecord::UserActStat>& actStat)
{
    ROLLLOG_DEBUG << "sRoundBetStat: "<< sRoundBetStat << endl;
    auto vecList = SEPSTR(sRoundBetStat, "|");
    if(vecList.size() != 4)
    {
        vecList.clear();
        vecList = {"0", "0", "0", "0"};
    }

    for(int i = 0; i < 4; i++)
    {
        if(isBet(actStat, i))
        {
            vecList[i] = I2S(S2I(vecList[i]) + (!bWin || winGold > 0 ? 1 : 0));
        }
    }

    return vecList[0] + "|" + vecList[1] + "|" + vecList[2] + "|"+ vecList[3];
}

string Processor::CalCompareStat(const string sCompareStat, long winGold, const map<int, gamerecord::UserActStat>& actStat)
{
    ROLLLOG_DEBUG << "sCompareStat: "<< sCompareStat << endl;
    if(isFold(actStat))
    {
        return sCompareStat.empty() ? "0|0" : sCompareStat ;
    }

    auto vecList = SEPSTR(sCompareStat, "|");
    if(vecList.size() != 2)
    {
        vecList.clear();
        vecList = {"0", "0"};
    }
    vecList[0] = I2S(S2I(vecList[0]) + 1);
    vecList[1] = I2S(S2I(vecList[1]) + (winGold > 0 ? 1 : 0));

    return vecList[0] + "|" + vecList[1];
}

string Processor::CalActStat(const string sAct, const int round, const map<int, gamerecord::UserActStat>& actStat)
{
    auto it = actStat.find(round);
    if(it == actStat.end())
    {
        return sAct;
    }

    ROLLLOG_DEBUG << "act stat sAct: "<< sAct << ", round: "<< round<< endl;
    map<int, int> mapActStat;
    auto vecList = SEPSTR(sAct, "|");
    for(auto item : vecList)
    {
        ROLLLOG_DEBUG << "act stat item: "<< item << endl;
        if(item.empty())
        {
            continue;
        }

        vector<int> vecInt;
        g_app.getOuterFactoryPtr()->splitInt(item, vecInt);
        if(vecInt.size() != 2)
        {
            continue;
        }
        auto subit = mapActStat.find(vecInt[0]);
        if(subit == mapActStat.end())
        {
            mapActStat.insert(std::make_pair(vecInt[0], vecInt[1]));
        }
        else
        {
            subit->second += vecInt[1];
        }
    }

   /* for(auto item : mapActStat)
    {
        LOG_DEBUG << "------act stat first:  "<< item.first << ", second: "<< item.second << endl;
    }*/

    for(auto item : it->second.mapActCount)
    {
        auto subit = mapActStat.find(item.first);
        if(subit == mapActStat.end())
        {
            mapActStat.insert(std::make_pair(item.first, item.second));
        }
        else
        {
            subit->second += item.second;
        }
    }

   /* for(auto item : mapActStat)
    {
        LOG_DEBUG << ">>>>>>act stat first:  "<< item.first << ", second: "<< item.second << endl;
    }*/

    string result;
    for(auto item : mapActStat)
    {
        string sub = I2S(item.first) + ":" + I2S(item.second) + "|";
        result += sub;
    }
    return result;
}

long Processor::getStatByRound(const string sStat, const unsigned int iRound)
{
    auto vecList = SEPSTR(sStat, "|");
    if(vecList.size() < iRound + 1)
    {
        return 0;
    }
    return S2I(vecList[iRound]);
}

long Processor::getActStatByAct(const map<string, string>& mapStatInfo, const vector<int>& actList, const vector<string>& roundList)
{
    map<string, map<int, int>> mapUserActStat;
    for(auto item : mapStatInfo)
    {
        if(item.first != "preflop" && item.first != "flop" && item.first != "turn" && item.first != "river")
        {
            continue;
        }
        auto vecList = SEPSTR(item.second, "|");
        map<int, int> mapActStat;
        for(auto item : vecList)
        {
            if(item.empty())
            {
                continue;
            }

            vector<int> vecInt;
            g_app.getOuterFactoryPtr()->splitInt(item, vecInt);
            if(vecInt.size() != 2)
            {
                continue;
            }
            auto subit = mapActStat.find(vecInt[0]);
            if(subit == mapActStat.end())
            {
                mapActStat.insert(std::make_pair(vecInt[0], vecInt[1]));
            }
            else
            {
                subit->second += vecInt[1];
            }
        }
        mapUserActStat.insert(std::make_pair(item.first, mapActStat));
    }

    long result = 0;
    for(auto round : roundList)
    {
        auto it = mapUserActStat.find(round);
        if(it != mapUserActStat.end())
        {
            for(auto act : actList)
            {
                auto subit = it->second.find(act);
                if(subit != it->second.end())
                {
                    result += subit->second;
                }
            }
        }
    }
    LOG_DEBUG << "result: "<< result << endl;
    return result;
}

int Processor::InsertUserGameStat(const long lUid, const string sBriefId, const int gameType, const gamerecord::UserStatInfo& userStatInfo)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    ROLLLOG_DEBUG << " sBriefId: "<< sBriefId << ", userStatInfo: "<< printTars(userStatInfo) << endl;
    if(sBriefId.empty())
    {
        return -1;
    }

    vector<map<string, string>> vecGameStat;
    iRet = SelectUserGameStat(lUid, sBriefId, gameType, 0, vecGameStat, true);
    if(iRet != 0 || vecGameStat.size() != 1)
    {
        ROLLLOG_ERROR << "get game stat err. iRet: "<< iRet<< ",vecGameStat size: "<< vecGameStat.size() << endl;
        return -2;
    }

    map<string, string> gameStat = vecGameStat[0];

    dataproxy::TWriteDataReq dataReq;
    dataReq.resetDefautlt();
    dataReq.keyName = I2S(E_REDIS_TYPE_LIST) + ":" + I2S(GAME_RECORD_STAT) + ":" + L2S(lUid);
    dataReq.operateType = E_REDIS_WRITE;
    dataReq.clusterInfo.resetDefautlt();
    dataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    dataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_USER_ID;
    dataReq.clusterInfo.frageFactor = lUid;
    dataReq.paraExt.resetDefautlt();
    dataReq.paraExt.queryType = E_REPLACE;

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = E_NONE;
    tfield.colName = "uid";
    tfield.colType = BIGINT;
    tfield.colValue = L2S(lUid);
    fields.push_back(tfield);
    tfield.colName = "brief_id";
    tfield.colType = STRING;
    tfield.colValue = sBriefId;
    fields.push_back(tfield);
    tfield.colName = "game_type";
    tfield.colType = INT;
    tfield.colValue = I2S(gameType);
    fields.push_back(tfield);
    tfield.colName = "rank";
    tfield.colType = INT;
    tfield.colValue = I2S(userStatInfo.iRank);
    fields.push_back(tfield);
    tfield.colName = "bet_num";
    tfield.colType = BIGINT;
    tfield.colValue = L2S((!gameStat["bet_num"].empty() ? S2I(gameStat["bet_num"]) : 0 ) + userStatInfo.betGold);
    fields.push_back(tfield);
    tfield.colName = "round_count";
    tfield.colType = INT;
    tfield.colValue = I2S((!gameStat["round_count"].empty() ? S2I(gameStat["round_count"]) : 0) + 1);
    fields.push_back(tfield);
    tfield.colName = "win_num";
    tfield.colType = BIGINT;
    tfield.colValue = L2S((!gameStat["win_num"].empty() ? S2I(gameStat["win_num"]) : 0) + userStatInfo.winGold);
    fields.push_back(tfield);
    tfield.colName = "win_count";
    tfield.colType = INT;
    tfield.colValue = I2S((!gameStat["win_count"].empty() ? S2I(gameStat["win_count"]) : 0) + (userStatInfo.winGold > 0 ? 1 : 0));
    fields.push_back(tfield);
    tfield.colName = "allin_win_count";
    tfield.colType = INT;
    tfield.colValue = I2S((!gameStat["allin_win_count"].empty() ? S2I(gameStat["allin_win_count"]) : 0) + (userStatInfo.winGold > 0 && isAllin(userStatInfo.actStat) ? 1 : 0));
    fields.push_back(tfield);
    tfield.colName = "win_pot_count";
    tfield.colType = INT;
    tfield.colValue = I2S((!gameStat["win_pot_count"].empty() ? S2I(gameStat["win_pot_count"]) : 0) + (userStatInfo.bWinPot ? 1 : 0));
    fields.push_back(tfield);
    tfield.colName = "bet_count";
    tfield.colType = INT;
    tfield.colValue = I2S((!gameStat["bet_count"].empty() ? S2I(gameStat["bet_count"]) : 0) + (isBet(userStatInfo.actStat) ? 1 : 0));
    fields.push_back(tfield);
    tfield.colName = "round_bet_count";
    tfield.colType = STRING;
    tfield.colValue = CalRoundBetStat(gameStat["round_bet_count"], userStatInfo.winGold, false, userStatInfo.actStat) ;
    fields.push_back(tfield);
    tfield.colName = "round_bet_win_count";
    tfield.colType = STRING;
    tfield.colValue = CalRoundBetStat(gameStat["round_bet_win_count"], userStatInfo.winGold, true, userStatInfo.actStat) ;
    fields.push_back(tfield);
    tfield.colName = "compare_count";
    tfield.colType = STRING;
    tfield.colValue = CalCompareStat(gameStat["compare_count"], userStatInfo.winGold, userStatInfo.actStat);
    fields.push_back(tfield);

    tfield.colName = "buy_num";
    tfield.colType = BIGINT;
    tfield.colValue = L2S(userStatInfo.buyGold);
    fields.push_back(tfield);
    tfield.colName = "insure_pay_num";
    tfield.colType = BIGINT;
    tfield.colValue = L2S((!gameStat["insure_pay_num"].empty() ? S2L(gameStat["insure_pay_num"]) : 0) + userStatInfo.insurePayGold);
    fields.push_back(tfield);
    tfield.colName = "insure_buy_num";
    tfield.colType = BIGINT;
    tfield.colValue = L2S((!gameStat["insure_buy_num"].empty() ? S2L(gameStat["insure_buy_num"]) : 0) + userStatInfo.insureBuyGold);
    fields.push_back(tfield);
    tfield.colName = "profit_num";
    tfield.colType = BIGINT;
    tfield.colValue = L2S((!gameStat["profit_num"].empty() ? S2L(gameStat["profit_num"]) : 0) + userStatInfo.lProfitGold);
    fields.push_back(tfield);

    tfield.colName = "preflop";
    tfield.colType = STRING;
    tfield.colValue = CalActStat(gameStat["preflop"], 0, userStatInfo.actStat);
    fields.push_back(tfield);
    tfield.colName = "flop";
    tfield.colType = STRING;
    tfield.colValue = CalActStat(gameStat["flop"], 1, userStatInfo.actStat);
    fields.push_back(tfield);
    tfield.colName = "turn";
    tfield.colType = STRING;
    tfield.colValue = CalActStat(gameStat["turn"], 2, userStatInfo.actStat);
    fields.push_back(tfield);
    tfield.colName = "river";
    tfield.colType = STRING;
    tfield.colValue = CalActStat(gameStat["river"], 3, userStatInfo.actStat);
    fields.push_back(tfield);
    tfield.colName = "log_time";
    tfield.colType = BIGINT;
    tfield.colValue = L2S(TNOW);
    fields.push_back(tfield);

    dataReq.fields = fields;

    dataproxy::TWriteDataRsp dataRsp;
    iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(lUid)->redisWrite(dataReq, dataRsp);
    if (iRet != 0 || dataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "insert game stat err, iRet: " << iRet << ", iResult: " << dataRsp.iResult << endl;
        return -2;
    }

    ROLLLOG_DEBUG << "insert game stat, iRet: " << iRet << ", dataRsp: " << printTars(dataRsp) << endl;

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

int Processor::SelectUserGameStat(const long lUid, const string sBriefId, const int gameType, const long logTime, vector<map<string, string>>& vecGameStat, bool insertFlag)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    dataproxy::TReadDataReq dataReq;
    dataReq.resetDefautlt();
    dataReq.keyName = I2S(E_REDIS_TYPE_LIST) + ":" + I2S(GAME_RECORD_STAT) + ":" + L2S(lUid);
    dataReq.operateType = E_REDIS_READ;
    dataReq.clusterInfo.resetDefautlt();
    dataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    dataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_USER_ID;
    dataReq.clusterInfo.frageFactor = lUid;
    dataReq.paraExt.resetDefautlt();
    dataReq.paraExt.subOperateType = E_REDIS_LIST_RANGE;//根据范围取数据
    dataReq.paraExt.start = 0;//起始下标从0开始
    dataReq.paraExt.end = -1;//终止最大结束下标为-1

    vector<string> vColName = {"uid", "brief_id", "game_type", "rank", "bet_num", "round_count", "win_num", "win_count", "allin_win_count", "win_pot_count", "preflop", "flop", "turn", "river", "log_time"};

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = E_NONE;

    for(auto col : vColName)
    {
        tfield.colName = col;
        fields.push_back(tfield);
    }
    dataReq.fields = fields;

    auto &conditionFields = dataReq.paraExt.fields;
    tfield.resetDefautlt();
    if(gameType > 0)
    {
        tfield.colValue = I2S(gameType);
        conditionFields["game_type"] = tfield;
    }
    if(logTime > 0)
    {
        tfield.colValue = L2S(logTime);
        conditionFields["log_time"] = tfield;
    }
    if(!sBriefId.empty())
    {
        tfield.colValue = sBriefId;
        conditionFields["brief_id"] = tfield;
    }

    dataproxy::TReadDataRsp dataRsp;
    iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(lUid)->redisRead(dataReq, dataRsp);
    if (iRet != 0 || dataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "get collect game err, iRet: " << iRet << ", iResult: " << dataRsp.iResult << endl;
        return -1;
    }

    for (auto it = dataRsp.fields.begin(); it != dataRsp.fields.end(); ++it)
    {
        map<string, string> mapGameStat;
        for (auto itTField = it->begin(); itTField != it->end(); ++itTField)
        {
            mapGameStat.insert(std::make_pair(itTField->colName, itTField->colValue));
        }
        vecGameStat.push_back(mapGameStat);
    }

    if(vecGameStat.size() == 0 && insertFlag)
    {
        map<string, string> mapGameStat;
        for(auto col : vColName)
        {
            mapGameStat.insert(std::make_pair(col, ""));
        }
        vecGameStat.push_back(mapGameStat);
    }

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

int Processor::InsertGameDetail(const gamerecord::GameDetail& gameDetail)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    dataproxy::TWriteDataReq dataReq;
    dataReq.resetDefautlt();
    dataReq.keyName = I2S(E_REDIS_TYPE_LIST) + ":" + I2S(GAME_RECORD_DETAIL) + ":" + gameDetail.sBriefId;
    dataReq.operateType = E_REDIS_WRITE;
    dataReq.clusterInfo.resetDefautlt();
    dataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    dataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_STRING;
    dataReq.clusterInfo.frageFactor = tars::hash<string>()(gameDetail.sBriefId);
    dataReq.paraExt.resetDefautlt();
    dataReq.paraExt.queryType = E_REPLACE;

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = E_NONE;
    tfield.colName = "brief_id";
    tfield.colType = STRING;
    tfield.colValue = gameDetail.sBriefId;
    fields.push_back(tfield);
    tfield.colName = "round";
    tfield.colType = INT;
    tfield.colValue = I2S(gameDetail.iRound);
    fields.push_back(tfield);
    tfield.colName = "buy_count";
    tfield.colType = BIGINT;
    tfield.colValue = L2S(gameDetail.lBuyCount);
    fields.push_back(tfield);
    tfield.colName = "pool";
    tfield.colType = BIGINT;
    tfield.colValue = L2S(gameDetail.lPool);
    fields.push_back(tfield);
    tfield.colName = "fee";
    tfield.colType = BIGINT;
    tfield.colValue = L2S(gameDetail.lFee);
    fields.push_back(tfield);
    tfield.colName = "insure_buy";
    tfield.colType = BIGINT;
    tfield.colValue = L2S(gameDetail.lInsureBuy);
    fields.push_back(tfield);
     tfield.colName = "insure_pay";
    tfield.colType = BIGINT;
    tfield.colValue = L2S(gameDetail.lInsurePay);
    fields.push_back(tfield);
    tfield.colName = "detail";
    tfield.colType = STRING;
    tfield.colValue = gameDetail.sGameDetail;
    fields.push_back(tfield);
    dataReq.fields = fields;

    dataproxy::TWriteDataRsp dataRsp;
    iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx( gameDetail.sBriefId)->redisWrite(dataReq, dataRsp);
    if (iRet != 0 || dataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "game detail err, iRet: " << iRet << ", iResult: " << dataRsp.iResult << endl;
        return -2;
    }

    ROLLLOG_DEBUG << "game detail, iRet: " << iRet << ", round: "<< gameDetail.iRound << ", dataRsp: " << printTars(dataRsp) << endl;

    for(auto item : gameDetail.userStatInfo)
    {
        iRet = InsertUserGameStat(item.first, gameDetail.sBriefId, gameDetail.iGameType, item.second);
        if(iRet != 0)
        {
            ROLLLOG_ERROR << "insert uaer game info err, iRet: " << iRet << ", lPlayerID: "<< item.first << endl;
            return iRet;
        }
    }

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

int Processor::SelectGameDetail(const string sBriefId, const int iRound, vector<map<string, string>>& vecGameDetail)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    dataproxy::TReadDataReq dataReq;
    dataReq.resetDefautlt();
    dataReq.keyName = I2S(E_REDIS_TYPE_LIST) + ":" + I2S(GAME_RECORD_DETAIL) + ":" + sBriefId;
    dataReq.operateType = E_REDIS_READ;
    dataReq.clusterInfo.resetDefautlt();
    dataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    dataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_STRING;
    dataReq.clusterInfo.frageFactor = tars::hash<string>()(sBriefId);
    dataReq.paraExt.resetDefautlt();
    dataReq.paraExt.subOperateType = E_REDIS_LIST_RANGE;//根据范围取数据
    dataReq.paraExt.start = 0;//起始下标从0开始
    dataReq.paraExt.end = -1;//终止最大结束下标为-1

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = E_NONE;
    tfield.colName = "round";
    fields.push_back(tfield);
    tfield.colName = "buy_count";
    fields.push_back(tfield);
    tfield.colName = "detail";
    fields.push_back(tfield);
    dataReq.fields = fields;

    auto &conditionFields = dataReq.paraExt.fields;
    tfield.resetDefautlt();
    tfield.colValue = sBriefId;
    conditionFields["brief_id"] = tfield;

    dataproxy::TReadDataRsp dataRsp;
    iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(sBriefId)->redisRead(dataReq, dataRsp);
    if (iRet != 0 || dataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "get game detail err, iRet: " << iRet << ", iResult: " << dataRsp.iResult << endl;
        return -1;
    }
    for (auto it = dataRsp.fields.begin(); it != dataRsp.fields.end(); ++it)
    {
        map<string, string> mapGameDetail;
        for (auto itTField = it->begin(); itTField != it->end(); ++itTField)
        {
            mapGameDetail.insert(std::make_pair(itTField->colName, itTField->colValue));
        }

        if((S2I(mapGameDetail["round"]) == iRound && iRound >= 0) || iRound < 0)
        {
            vecGameDetail.push_back(mapGameDetail);
        }
    }

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

int Processor::InsertGameBrief(const gamerecord::GameBrief& gameBrief)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    if(gameBrief.sBriefId.empty())
    {
        ROLLLOG_ERROR << "report data err. sBriefId: "<< gameBrief.sBriefId << endl;
        return -1;
    }

    vector<map<string, string>> vecGameDetail;
    SelectGameDetail(gameBrief.sBriefId, -1, vecGameDetail);
    if(vecGameDetail.size() == 0)
    {
        ROLLLOG_DEBUG << "not game detail. sBriefId: "<< gameBrief.sBriefId << endl;
        //return 0;
    }

    dataproxy::TWriteDataReq dataReq;
    dataReq.resetDefautlt();
    dataReq.keyName = I2S(E_REDIS_TYPE_HASH) + ":" + I2S(GAME_RECORD_BRIEF) + ":" + gameBrief.sBriefId;
    dataReq.operateType = E_REDIS_WRITE;
    dataReq.clusterInfo.resetDefautlt();
    dataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    dataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_STRING;
    dataReq.clusterInfo.frageFactor = tars::hash<string>()(gameBrief.sBriefId);
    dataReq.paraExt.resetDefautlt();
    dataReq.paraExt.queryType = E_REPLACE;

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = E_NONE;
    tfield.colName = "brief_id";
    tfield.colType = STRING;
    tfield.colValue = gameBrief.sBriefId;
    fields.push_back(tfield);
    tfield.colName = "creater_id";
    tfield.colType = BIGINT;
    tfield.colValue = I2S(gameBrief.lCreaterId);
    fields.push_back(tfield);
    tfield.colName = "room_key";
    tfield.colType = STRING;
    tfield.colValue = gameBrief.sRoomKey;
    fields.push_back(tfield);
    tfield.colName = "room_name";
    tfield.colType = STRING;
    tfield.colValue = gameBrief.sRoomName;
    fields.push_back(tfield);
    tfield.colName = "game_type";
    tfield.colType = INT;
    tfield.colValue = I2S(gameBrief.iGameType);
    fields.push_back(tfield);
    tfield.colName = "bet_id";
    tfield.colType = INT;
    tfield.colValue = I2S(gameBrief.iBetId);
    fields.push_back(tfield);
    tfield.colName = "fee";
    tfield.colType = BIGINT;
    tfield.colValue = L2S(gameBrief.lFee);
    fields.push_back(tfield);
    tfield.colName = "insure";
    tfield.colType = INT;
    tfield.colValue = gameBrief.bInsure ? "1" : "0";
    fields.push_back(tfield);
    tfield.colName = "club_id";
    tfield.colType = INT;
    tfield.colValue = I2S(gameBrief.iClubId);
    fields.push_back(tfield);
    tfield.colName = "club_name";
    tfield.colType = STRING;
    tfield.colValue = gameBrief.sClubName;
    fields.push_back(tfield);
    tfield.colName = "game_time";
    tfield.colType = INT;
    tfield.colValue = I2S(gameBrief.lGameTime);
    fields.push_back(tfield);
    tfield.colName = "small_blind";
    tfield.colType = BIGINT;
    tfield.colValue = L2S(gameBrief.lSmallBlind);
    fields.push_back(tfield);
    tfield.colName = "big_blind";
    tfield.colType = BIGINT;
    tfield.colValue = L2S(gameBrief.lBigBlind);
    fields.push_back(tfield);
    tfield.colName = "front_bet";
    tfield.colType = BIGINT;
    tfield.colValue = L2S(gameBrief.lFrontBet);
    fields.push_back(tfield);
    tfield.colName = "max_seat";
    tfield.colType = INT;
    tfield.colValue = I2S(gameBrief.iMaxSeat);
    fields.push_back(tfield);

    Pb::GameInsureInfoList insureInfoList;
    for(auto sDetail : gameBrief.vecInsureDetail)
    {
        Pb::GameInsureInfo info;
        info.ParseFromString(sDetail);
        auto ptr = insureInfoList.add_insurelist();
        ptr->CopyFrom(info);
    }
    string sInsureInfoList;
    insureInfoList.SerializeToString(&sInsureInfoList);
    tfield.colName = "insure_list";
    tfield.colType = STRING;
    tfield.colValue = sInsureInfoList;
    fields.push_back(tfield);

    Pb::GameApplyInfoList applyInfoList;
    for(auto info : gameBrief.sApplyList)
    {
        auto ptr = applyInfoList.add_applylist();
        ptr->set_makerid(info.makerId);
        ptr->set_makername(info.makerName);
        ptr->set_istate(info.iState);
        ptr->mutable_userinfo()->set_lplayerid(info.lPlayerID);
        ptr->mutable_userinfo()->set_snickname(info.sNickName);
        ptr->mutable_userinfo()->set_sheadstr(info.sHeadStr);
        ptr->mutable_userinfo()->set_ltakenin(info.buyCount);
    }
    string sApplyInfoList;
    applyInfoList.SerializeToString(&sApplyInfoList);
    tfield.colName = "apply_list";
    tfield.colType = STRING;
    tfield.colValue = sApplyInfoList;
    fields.push_back(tfield);

    string sPlayers = "|";
    Pb::GameBuyInInfoList buyinInfoList;
    for(auto item : gameBrief.mUserBuyin)
    {
        sPlayers += L2S(item.first) + "|";
        auto ptr = buyinInfoList.add_buyinlist();
        ptr->set_lplayerid(item.first);
        ptr->set_ltakenin(item.second);
        auto it = gameBrief.mUserRank.find(item.first);
        if(it != gameBrief.mUserRank.end())
        {
            ptr->set_irank(it->second);
        }

        // 大菠萝专用
        auto tempUser = gameBrief.mTempBuyIn.find(item.first);
        if (tempUser != gameBrief.mTempBuyIn.end())
        {
            ptr->set_snickname(tempUser->second.sNickName);
            ptr->set_sheadstr(tempUser->second.sHeadStr);
            ptr->set_ltakenin(tempUser->second.buyCount);
            ptr->set_ltakenout(tempUser->second.makerId);
            ptr->set_ihand(tempUser->second.iState);
        }
    }
    string sBuyInInfoList;
    buyinInfoList.SerializeToString(&sBuyInInfoList);
    tfield.colName = "buyin_list";
    tfield.colType = STRING;
    tfield.colValue = sBuyInInfoList;
    fields.push_back(tfield);

    tfield.colName = "player_list";
    tfield.colType = STRING;
    tfield.colValue = sPlayers;
    fields.push_back(tfield);

    tfield.colName = "log_time";
    tfield.colType = BIGINT;
    tfield.colValue = L2S(TNOW);
    fields.push_back(tfield);

    tfield.colName = "room_option";
    tfield.colType = STRING;
    tfield.colValue = gameBrief.roomOption;
    fields.push_back(tfield);

    dataReq.fields = fields;

    dataproxy::TWriteDataRsp dataRsp;
    iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx( gameBrief.sBriefId)->redisWrite(dataReq, dataRsp);
    if (iRet != 0 || dataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "insert game brief err, iRet: " << iRet << ", iResult: " << dataRsp.iResult << endl;
        return -2;
    }

    ROLLLOG_DEBUG << "insert game brief, iRet: " << iRet << ", dataRsp: " << printTars(dataRsp) << endl;

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

int Processor::SelectGameBrief(const string sBriefId, vector<map<string, string>>& vecGameBrief)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    if(sBriefId.empty())
    {
        ROLLLOG_ERROR << "select game brief err. sBriefId: "<< sBriefId << endl;
        return -1;
    }

    dataproxy::TReadDataReq dataReq;
    dataReq.resetDefautlt();
    dataReq.keyName = I2S(E_REDIS_TYPE_LIST) + ":" + I2S(GAME_RECORD_BRIEF) + ":" + sBriefId;
    dataReq.operateType = E_REDIS_READ;
    dataReq.clusterInfo.resetDefautlt();
    dataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    dataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_STRING;
    dataReq.clusterInfo.frageFactor = tars::hash<string>()(sBriefId);
    dataReq.paraExt.resetDefautlt();
    dataReq.paraExt.subOperateType = E_REDIS_LIST_RANGE;//根据范围取数据
    dataReq.paraExt.start = 0;//起始下标从0开始
    dataReq.paraExt.end = -1;//终止最大结束下标为-1

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = E_NONE;
    tfield.colName = "brief_id";
    fields.push_back(tfield);
    tfield.colName = "creater_id";
    fields.push_back(tfield);
    tfield.colName = "room_key";
    fields.push_back(tfield);
    tfield.colName = "room_name";
    fields.push_back(tfield);
    tfield.colName = "game_type";
    fields.push_back(tfield);
    tfield.colName = "bet_id";
    fields.push_back(tfield);
    tfield.colName = "fee";
    fields.push_back(tfield);
    tfield.colName = "insure";
    fields.push_back(tfield);
    tfield.colName = "club_id";
    fields.push_back(tfield);
    tfield.colName = "club_name";
    fields.push_back(tfield);
    tfield.colName = "game_time";
    fields.push_back(tfield);
    tfield.colName = "small_blind";
    fields.push_back(tfield);
    tfield.colName = "big_blind";
    fields.push_back(tfield);
    tfield.colName = "front_bet";
    fields.push_back(tfield);
    tfield.colName = "insure_list";
    fields.push_back(tfield);
    tfield.colName = "apply_list";
    fields.push_back(tfield);
    tfield.colName = "player_list";
    fields.push_back(tfield);
    tfield.colName = "buyin_list";
    fields.push_back(tfield);
    tfield.colName = "max_seat";
    fields.push_back(tfield);
    tfield.colName = "log_time";
    fields.push_back(tfield);
    tfield.colName = "room_option";
    fields.push_back(tfield);
    dataReq.fields = fields;

    auto &conditionFields = dataReq.paraExt.fields;
    tfield.resetDefautlt();
    tfield.colValue = sBriefId;
    conditionFields["brief_id"] = tfield;

    dataproxy::TReadDataRsp dataRsp;
    iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(sBriefId)->redisRead(dataReq, dataRsp);
    if (iRet != 0 || dataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "get game brief err, iRet: " << iRet << ", iResult: " << dataRsp.iResult << endl;
        return -1;
    }
    for (auto it = dataRsp.fields.begin(); it != dataRsp.fields.end(); ++it)
    {
        map<string, string> mapGameBrief;
        for (auto itTField = it->begin(); itTField != it->end(); ++itTField)
        {
            mapGameBrief.insert(std::make_pair(itTField->colName, itTField->colValue));
        }
        vecGameBrief.push_back(mapGameBrief);
    }

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

int Processor::SelectGameBriefByUid(const long lUid, const int iDays, const int iGameType, vector<map<string, string>>& vecGameBrief)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    string table_name = "tb_game_brief";
    std::vector<string> col_name = {"brief_id", "creater_id", "room_key", "room_name", "game_type", "bet_id", "fee", "insure", "club_id", "club_name", "game_time", "small_blind", "big_blind", "front_bet", "apply_list", "player_list", "buyin_list", "room_option", "log_time"};
    std::vector<vector<string>> whlist = {
            {"player_list", "6", "%|" + L2S(lUid) + "|%"},//like
            {"game_type", "0", I2S(iGameType)},//eq
            {"log_time", "4", L2S((TNOW - TNOW % (24* 3600)) - iDays * 24* 3600)},//>=
        };
    dbagent::TDBReadRsp dataRsp;
    iRet = readDataFromDB(lUid, table_name, col_name, whlist, "log_time", dataRsp);
    if(iRet != 0)
    {
        LOG_ERROR<<"select tb_game_brief err!"<< endl;
        return iRet;
    }

    for (auto it = dataRsp.records.begin(); it != dataRsp.records.end(); ++it)
    {
        map<string, string> mapGameBrief;
        for (auto itfield = it->begin(); itfield != it->end(); ++itfield)
        {
            mapGameBrief.insert(std::make_pair(itfield->colName, itfield->colValue));
        }
        vecGameBrief.push_back(mapGameBrief);
    }

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

int Processor::InsertGameDetailCowBoy(const gamerecord::GameDetailCowBoy& gameDetail)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    LOG_DEBUG << "GameDetailCowBoy: "<< printTars(gameDetail)<< endl;

    if(gameDetail.sBriefId.empty())
    {
        ROLLLOG_ERROR << "report data err. sBriefId: "<< gameDetail.sBriefId << endl;
        return -1;
    }

    dataproxy::TWriteDataReq dataReq;
    dataReq.resetDefautlt();
    dataReq.keyName = I2S(E_REDIS_TYPE_HASH) + ":" + I2S(GAME_RECORD_DETAIL_COWBOY) + ":" + gameDetail.sBriefId;
    dataReq.operateType = E_REDIS_INSERT;
    dataReq.clusterInfo.resetDefautlt();
    dataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    dataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_STRING;
    dataReq.clusterInfo.frageFactor = tars::hash<string>()(gameDetail.sBriefId);

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = E_NONE;
    tfield.colName = "brief_id";
    tfield.colType = STRING;
    tfield.colValue = gameDetail.sBriefId;
    fields.push_back(tfield);
    tfield.colName = "uid";
    tfield.colType = BIGINT;
    tfield.colValue = L2S(gameDetail.lPlayerId);
    fields.push_back(tfield);
    tfield.colName = "room_key";
    tfield.colType = STRING;
    tfield.colValue = gameDetail.sRoomKey;
    fields.push_back(tfield);
    tfield.colName = "round";
    tfield.colType = INT;
    tfield.colValue = I2S(gameDetail.iRound);
    fields.push_back(tfield);
    fields.push_back(tfield);
    tfield.colName = "bets";
    tfield.colType = BIGINT;
    tfield.colValue = L2S(gameDetail.lBetValue);
    fields.push_back(tfield);
    fields.push_back(tfield);
    tfield.colName = "pays";
    tfield.colType = BIGINT;
    tfield.colValue = L2S(gameDetail.lPayValue);
    fields.push_back(tfield);
    fields.push_back(tfield);
    tfield.colName = "fees";
    tfield.colType = BIGINT;
    tfield.colValue = L2S(gameDetail.lFeeValue);
    fields.push_back(tfield);

    Pb::GameDetailCowBoy detail;
    detail.set_sbriefid(gameDetail.sBriefId);
    detail.set_lplayerid(gameDetail.lPlayerId);
    detail.set_sroomkey(gameDetail.sRoomKey);
    detail.set_iround(gameDetail.iRound);
    detail.set_ireal(gameDetail.iReal);
    detail.set_lbetvalue(gameDetail.lBetValue);
    detail.set_lpayvalue(gameDetail.lPayValue);
    detail.set_lfeevalue(gameDetail.lFeeValue);

    for (auto item : gameDetail.vCowBoyCard)
    {
        detail.add_cowboycards(item);
    }
    for (auto item : gameDetail.vCowGirlCard)
    {
        detail.add_cowgirlcards(item);
    }
    for (auto item : gameDetail.vCommonCard)
    {
        detail.add_commoncards(item);
    }
    for (auto item : gameDetail.vAreaWin)
    {
        detail.add_areawins(item);
    }

    for (auto item : gameDetail.mBetInfo)
    {
        auto pItem = detail.add_betinfos();
        pItem->set_iareaid(item.first);
        pItem->set_lamount(item.second);
    }
    for (auto item : gameDetail.mPayInfo)
    {
        auto pItem = detail.add_payinfos();
        pItem->set_iareaid(item.first);
        pItem->set_lamount(item.second);
    }
    for (auto item : gameDetail.mOddsInfo)
    {
        auto pItem = detail.add_oddsinfos();
        pItem->set_iareaid(item.first);
        pItem->set_lamount(item.second);
    }
    
    string sDetail;
    detail.SerializeToString(&sDetail);
    tfield.colName = "detail";
    tfield.colType = STRING;
    tfield.colValue = sDetail;
    fields.push_back(tfield);

    dataReq.fields = fields;

    dataproxy::TWriteDataRsp dataRsp;
    iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(gameDetail.sBriefId)->redisWrite(dataReq, dataRsp);
    if (iRet != 0 || dataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "insert cowboy game detail err, iRet: " << iRet << ", iResult: " << dataRsp.iResult << endl;
        return -2;
    }

    ROLLLOG_DEBUG << "insert cowboy game detail, iRet: " << iRet << ", dataRsp: " << printTars(dataRsp) << endl;

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

int Processor::SelectGameDetailCowBoyByUid(const long lUid, const string sBriefId, vector<map<string, string>>& vecGameDetail)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    dataproxy::TReadDataReq dataReq;
    dataReq.resetDefautlt();
    dataReq.keyName = I2S(E_REDIS_TYPE_LIST) + ":" + I2S(GAME_RECORD_DETAIL_COWBOY) + ":" + sBriefId;
    dataReq.operateType = E_REDIS_READ;
    dataReq.clusterInfo.resetDefautlt();
    dataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    dataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_STRING;
    dataReq.clusterInfo.frageFactor = tars::hash<string>()(sBriefId);
    dataReq.paraExt.resetDefautlt();
    dataReq.paraExt.subOperateType = E_REDIS_LIST_RANGE;//根据范围取数据
    dataReq.paraExt.start = 0;//起始下标从0开始
    dataReq.paraExt.end = -1;//终止最大结束下标为-1

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = E_NONE;
    tfield.colName = "brief_id";
    fields.push_back(tfield);
    tfield.colName = "uid";
    fields.push_back(tfield);
    tfield.colName = "room_key";
    fields.push_back(tfield);
    tfield.colName = "round";
    fields.push_back(tfield);
    tfield.colName = "detail";
    fields.push_back(tfield);
    dataReq.fields = fields;

    auto &conditionFields = dataReq.paraExt.fields;
    tfield.resetDefautlt();
    tfield.colValue = sBriefId;
    conditionFields["brief_id"] = tfield;
    tfield.colValue = L2S(lUid);
    conditionFields["uid"] = tfield;

    dataproxy::TReadDataRsp dataRsp;
    iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(sBriefId)->redisRead(dataReq, dataRsp);
    if (iRet != 0 || dataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "get cowboy game detail err, iRet: " << iRet << ", iResult: " << dataRsp.iResult << endl;
        return -1;
    }
    for (auto it = dataRsp.fields.begin(); it != dataRsp.fields.end(); ++it)
    {
        map<string, string> mapGameDetail;
        for (auto itfield = it->begin(); itfield != it->end(); ++itfield)
        {
            mapGameDetail.insert(std::make_pair(itfield->colName, itfield->colValue));
        }
        vecGameDetail.push_back(mapGameDetail);
    }

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

int Processor::InsertGameRecordCowBoy(const gamerecord::GameRecordCowBoy& gameRecord)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    LOG_DEBUG << "InsertGameRecordCowBoy: "<< printTars(gameRecord)<< endl;

    if(gameRecord.sBriefId.empty())
    {
        ROLLLOG_ERROR << "report data err. sBriefId: "<< gameRecord.sBriefId << endl;
        return -1;
    }

    dataproxy::TWriteDataReq dataReq;
    dataReq.resetDefautlt();
    dataReq.keyName = I2S(E_REDIS_TYPE_HASH) + ":" + I2S(GAME_RECORD_RECORD_COWBOY) + ":" + gameRecord.sBriefId;
    dataReq.operateType = E_REDIS_INSERT;
    dataReq.clusterInfo.resetDefautlt();
    dataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    dataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_STRING;
    dataReq.clusterInfo.frageFactor = tars::hash<string>()(gameRecord.sBriefId);

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = E_NONE;
    tfield.colName = "brief_id";
    tfield.colType = STRING;
    tfield.colValue = gameRecord.sBriefId;
    fields.push_back(tfield);
    tfield.colName = "uid";
    tfield.colType = BIGINT;
    tfield.colValue = L2S(gameRecord.lPlayerId);
    fields.push_back(tfield);
    tfield.colName = "nickname";
    tfield.colType = STRING;
    tfield.colValue = gameRecord.sNickname;
    fields.push_back(tfield);
    tfield.colName = "head_str";
    tfield.colType = STRING;
    tfield.colValue = gameRecord.sHeadStr;
    fields.push_back(tfield);
    tfield.colName = "round";
    tfield.colType = INT;
    tfield.colValue = I2S(gameRecord.iRound);
    fields.push_back(tfield);
    tfield.colName = "buy_in";
    tfield.colType = BIGINT;
    tfield.colValue = I2S(gameRecord.lBuyIn);
    fields.push_back(tfield);
    tfield.colName = "buy_out";
    tfield.colType = BIGINT;
    tfield.colValue = I2S(gameRecord.lBuyOut);
    fields.push_back(tfield);
    tfield.colName = "bets";
    tfield.colType = BIGINT;
    tfield.colValue = I2S(gameRecord.lBets);
    fields.push_back(tfield);
    tfield.colName = "wins";
    tfield.colType = BIGINT;
    tfield.colValue = I2S(gameRecord.lWins);
    fields.push_back(tfield);

    dataReq.fields = fields;

    dataproxy::TWriteDataRsp dataRsp;
    iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(gameRecord.sBriefId)->redisWrite(dataReq, dataRsp);
    if (iRet != 0 || dataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "insert cowboy game record err, iRet: " << iRet << ", iResult: " << dataRsp.iResult << endl;
        return -2;
    }

    ROLLLOG_DEBUG << "insert cowboy game record, iRet: " << iRet << ", dataRsp: " << printTars(dataRsp) << endl;

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

int Processor::SelectGameRecordCowBoy(const string sBriefId, vector<map<string, string>>& vecGameRecord)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    dataproxy::TReadDataReq dataReq;
    dataReq.resetDefautlt();
    dataReq.keyName = I2S(E_REDIS_TYPE_LIST) + ":" + I2S(GAME_RECORD_RECORD_COWBOY) + ":" + sBriefId;
    dataReq.operateType = E_REDIS_READ;
    dataReq.clusterInfo.resetDefautlt();
    dataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    dataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_STRING;
    dataReq.clusterInfo.frageFactor = tars::hash<string>()(sBriefId);
    dataReq.paraExt.resetDefautlt();
    dataReq.paraExt.subOperateType = E_REDIS_LIST_RANGE;//根据范围取数据
    dataReq.paraExt.start = 0;//起始下标从0开始
    dataReq.paraExt.end = -1;//终止最大结束下标为-1

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = E_NONE;
    tfield.colName = "brief_id";
    fields.push_back(tfield);
    tfield.colName = "uid";
    fields.push_back(tfield);
    tfield.colName = "nickname";
    fields.push_back(tfield);
    tfield.colName = "heand_str";
    fields.push_back(tfield);
    tfield.colName = "round";
    fields.push_back(tfield);
    tfield.colName = "buy_in";
    fields.push_back(tfield);
    tfield.colName = "buy_out";
    fields.push_back(tfield);
    tfield.colName = "bets";
    fields.push_back(tfield);
    tfield.colName = "wins";
    fields.push_back(tfield);
    dataReq.fields = fields;

    auto &conditionFields = dataReq.paraExt.fields;
    tfield.resetDefautlt();
    tfield.colValue = sBriefId;
    conditionFields["brief_id"] = tfield;

    dataproxy::TReadDataRsp dataRsp;
    iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(sBriefId)->redisRead(dataReq, dataRsp);
    if (iRet != 0 || dataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "get cowboy game detail err, iRet: " << iRet << ", iResult: " << dataRsp.iResult << endl;
        return -1;
    }
    for (auto it = dataRsp.fields.begin(); it != dataRsp.fields.end(); ++it)
    {
        map<string, string> mapGameRecord;
        for (auto itfield = it->begin(); itfield != it->end(); ++itfield)
        {
            mapGameRecord.insert(std::make_pair(itfield->colName, itfield->colValue));
        }
        vecGameRecord.push_back(mapGameRecord);
    }

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

int Processor::InsertGameDetailCowBoyBanker(const gamerecord::GameDetailCowBoy& gameDetail)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    LOG_DEBUG << "InsertGameDetailCowBoyBanker: "<< printTars(gameDetail)<< endl;

    if(gameDetail.sBriefId.empty())
    {
        ROLLLOG_ERROR << "report data err. sBriefId: "<< gameDetail.sBriefId << endl;
        return -1;
    }

    dataproxy::TWriteDataReq dataReq;
    dataReq.resetDefautlt();
    dataReq.keyName = I2S(E_REDIS_TYPE_HASH) + ":" + I2S(GAME_RECORD_DETAIL_COWBOY_BANKER) + ":" + gameDetail.sBriefId;
    dataReq.operateType = E_REDIS_INSERT;
    dataReq.clusterInfo.resetDefautlt();
    dataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    dataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_STRING;
    dataReq.clusterInfo.frageFactor = tars::hash<string>()(gameDetail.sBriefId);

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = E_NONE;
    tfield.colName = "brief_id";
    tfield.colType = STRING;
    tfield.colValue = gameDetail.sBriefId;
    fields.push_back(tfield);
    tfield.colName = "uid";
    tfield.colType = BIGINT;
    tfield.colValue = L2S(gameDetail.lPlayerId);
    fields.push_back(tfield);
    tfield.colName = "room_key";
    tfield.colType = STRING;
    tfield.colValue = gameDetail.sRoomKey;
    fields.push_back(tfield);
    tfield.colName = "round";
    tfield.colType = INT;
    tfield.colValue = I2S(gameDetail.iRound);
    fields.push_back(tfield);
    fields.push_back(tfield);
    tfield.colName = "bets";
    tfield.colType = BIGINT;
    tfield.colValue = L2S(gameDetail.lBetValue);
    fields.push_back(tfield);
    fields.push_back(tfield);
    tfield.colName = "pays";
    tfield.colType = BIGINT;
    tfield.colValue = L2S(gameDetail.lPayValue);
    fields.push_back(tfield);
    fields.push_back(tfield);
    tfield.colName = "fees";
    tfield.colType = BIGINT;
    tfield.colValue = L2S(gameDetail.lFeeValue);
    fields.push_back(tfield);

    Pb::GameDetailCowBoy detail;
    detail.set_sbriefid(gameDetail.sBriefId);
    detail.set_lplayerid(gameDetail.lPlayerId);
    detail.set_sroomkey(gameDetail.sRoomKey);
    detail.set_iround(gameDetail.iRound);
    detail.set_ireal(gameDetail.iReal);
    detail.set_lbetvalue(gameDetail.lBetValue);
    detail.set_lpayvalue(gameDetail.lPayValue);
    detail.set_lfeevalue(gameDetail.lFeeValue);

    for (auto item : gameDetail.vCowBoyCard)
    {
        detail.add_cowboycards(item);
    }
    for (auto item : gameDetail.vCowGirlCard)
    {
        detail.add_cowgirlcards(item);
    }
    for (auto item : gameDetail.vCommonCard)
    {
        detail.add_commoncards(item);
    }
    for (auto item : gameDetail.vAreaWin)
    {
        detail.add_areawins(item);
    }

    for (auto item : gameDetail.mBetInfo)
    {
        auto pItem = detail.add_betinfos();
        pItem->set_iareaid(item.first);
        pItem->set_lamount(item.second);
    }
    for (auto item : gameDetail.mPayInfo)
    {
        auto pItem = detail.add_payinfos();
        pItem->set_iareaid(item.first);
        pItem->set_lamount(item.second);
    }
    for (auto item : gameDetail.mOddsInfo)
    {
        auto pItem = detail.add_oddsinfos();
        pItem->set_iareaid(item.first);
        pItem->set_lamount(item.second);
    }
    
    string sDetail;
    detail.SerializeToString(&sDetail);
    tfield.colName = "detail";
    tfield.colType = STRING;
    tfield.colValue = sDetail;
    fields.push_back(tfield);

    dataReq.fields = fields;

    dataproxy::TWriteDataRsp dataRsp;
    iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(gameDetail.sBriefId)->redisWrite(dataReq, dataRsp);
    if (iRet != 0 || dataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "insert banker cowboy game detail err, iRet: " << iRet << ", iResult: " << dataRsp.iResult << endl;
        return -2;
    }

    ROLLLOG_DEBUG << "insert banker cowboy game detail, iRet: " << iRet << ", dataRsp: " << printTars(dataRsp) << endl;

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

int Processor::SelectGameDetailCowBoyBankerByUid(const long lUid, const string sBriefId, vector<map<string, string>>& vecGameDetail)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    dataproxy::TReadDataReq dataReq;
    dataReq.resetDefautlt();
    dataReq.keyName = I2S(E_REDIS_TYPE_LIST) + ":" + I2S(GAME_RECORD_DETAIL_COWBOY_BANKER) + ":" + sBriefId;
    dataReq.operateType = E_REDIS_READ;
    dataReq.clusterInfo.resetDefautlt();
    dataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    dataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_STRING;
    dataReq.clusterInfo.frageFactor = tars::hash<string>()(sBriefId);
    dataReq.paraExt.resetDefautlt();
    dataReq.paraExt.subOperateType = E_REDIS_LIST_RANGE;//根据范围取数据
    dataReq.paraExt.start = 0;//起始下标从0开始
    dataReq.paraExt.end = -1;//终止最大结束下标为-1

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = E_NONE;
    tfield.colName = "brief_id";
    fields.push_back(tfield);
    tfield.colName = "uid";
    fields.push_back(tfield);
    tfield.colName = "room_key";
    fields.push_back(tfield);
    tfield.colName = "round";
    fields.push_back(tfield);
    tfield.colName = "detail";
    fields.push_back(tfield);
    dataReq.fields = fields;

    auto &conditionFields = dataReq.paraExt.fields;
    tfield.resetDefautlt();
    tfield.colValue = sBriefId;
    conditionFields["brief_id"] = tfield;
    tfield.colValue = L2S(lUid);
    conditionFields["uid"] = tfield;

    dataproxy::TReadDataRsp dataRsp;
    iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(sBriefId)->redisRead(dataReq, dataRsp);
    if (iRet != 0 || dataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "get cowboy banker game detail err, iRet: " << iRet << ", iResult: " << dataRsp.iResult << endl;
        return -1;
    }
    for (auto it = dataRsp.fields.begin(); it != dataRsp.fields.end(); ++it)
    {
        map<string, string> mapGameDetail;
        for (auto itfield = it->begin(); itfield != it->end(); ++itfield)
        {
            mapGameDetail.insert(std::make_pair(itfield->colName, itfield->colValue));
        }
        vecGameDetail.push_back(mapGameDetail);
    }

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

int Processor::InsertGameRecordCowBoyBanker(const gamerecord::GameRecordCowBoyBanker& gameRecord)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    LOG_DEBUG << "InsertGameRecordCowBoyBanker: "<< printTars(gameRecord)<< endl;

    if(gameRecord.sBriefId.empty())
    {
        ROLLLOG_ERROR << "report data err. sBriefId: "<< gameRecord.sBriefId << endl;
        return -1;
    }

    dataproxy::TWriteDataReq dataReq;
    dataReq.resetDefautlt();
    dataReq.keyName = I2S(E_REDIS_TYPE_HASH) + ":" + I2S(GAME_RECORD_RECORD_COWBOY_BANKER) + ":" + gameRecord.sBriefId;
    dataReq.operateType = E_REDIS_INSERT;
    dataReq.clusterInfo.resetDefautlt();
    dataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    dataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_STRING;
    dataReq.clusterInfo.frageFactor = tars::hash<string>()(gameRecord.sBriefId);

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = E_NONE;
    tfield.colName = "brief_id";
    tfield.colType = STRING;
    tfield.colValue = gameRecord.sBriefId;
    fields.push_back(tfield);
    tfield.colName = "uid";
    tfield.colType = BIGINT;
    tfield.colValue = L2S(gameRecord.lPlayerId);
    fields.push_back(tfield);
    tfield.colName = "nickname";
    tfield.colType = STRING;
    tfield.colValue = gameRecord.sNickname;
    fields.push_back(tfield);
    tfield.colName = "head_str";
    tfield.colType = STRING;
    tfield.colValue = gameRecord.sHeadStr;
    fields.push_back(tfield);
    tfield.colName = "round";
    tfield.colType = INT;
    tfield.colValue = I2S(gameRecord.iRound);
    fields.push_back(tfield);
    tfield.colName = "buy_in";
    tfield.colType = BIGINT;
    tfield.colValue = I2S(gameRecord.lBuyIn);
    fields.push_back(tfield);
    tfield.colName = "buy_out";
    tfield.colType = BIGINT;
    tfield.colValue = I2S(gameRecord.lBuyOut);
    fields.push_back(tfield);
    tfield.colName = "bets";
    tfield.colType = BIGINT;
    tfield.colValue = I2S(gameRecord.lBets);
    fields.push_back(tfield);
     tfield.colName = "pays";
    tfield.colType = BIGINT;
    tfield.colValue = I2S(gameRecord.lPays);
    fields.push_back(tfield);
     tfield.colName = "fees";
    tfield.colType = BIGINT;
    tfield.colValue = I2S(gameRecord.lFees);
    fields.push_back(tfield);
    tfield.colName = "wins";
    tfield.colType = BIGINT;
    tfield.colValue = I2S(gameRecord.lWins);
    fields.push_back(tfield);

    dataReq.fields = fields;

    dataproxy::TWriteDataRsp dataRsp;
    iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(gameRecord.sBriefId)->redisWrite(dataReq, dataRsp);
    if (iRet != 0 || dataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "insert banker cowboy game record err, iRet: " << iRet << ", iResult: " << dataRsp.iResult << endl;
        return -2;
    }

    ROLLLOG_DEBUG << "insert banker cowboy game record, iRet: " << iRet << ", dataRsp: " << printTars(dataRsp) << endl;

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

int Processor::SelectGameRecordCowBoyBanker(const string sBriefId, vector<map<string, string>>& vecGameRecord)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    dataproxy::TReadDataReq dataReq;
    dataReq.resetDefautlt();
    dataReq.keyName = I2S(E_REDIS_TYPE_LIST) + ":" + I2S(GAME_RECORD_RECORD_COWBOY_BANKER) + ":" + sBriefId;
    dataReq.operateType = E_REDIS_READ;
    dataReq.clusterInfo.resetDefautlt();
    dataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    dataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_STRING;
    dataReq.clusterInfo.frageFactor = tars::hash<string>()(sBriefId);
    dataReq.paraExt.resetDefautlt();
    dataReq.paraExt.subOperateType = E_REDIS_LIST_RANGE;//根据范围取数据
    dataReq.paraExt.start = 0;//起始下标从0开始
    dataReq.paraExt.end = -1;//终止最大结束下标为-1

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = E_NONE;
    tfield.colName = "brief_id";
    fields.push_back(tfield);
    tfield.colName = "uid";
    fields.push_back(tfield);
    tfield.colName = "nickname";
    fields.push_back(tfield);
    tfield.colName = "heand_str";
    fields.push_back(tfield);
    tfield.colName = "round";
    fields.push_back(tfield);
    tfield.colName = "buy_in";
    fields.push_back(tfield);
    tfield.colName = "buy_out";
    fields.push_back(tfield);
    tfield.colName = "bets";
    fields.push_back(tfield);
    tfield.colName = "wins";
    fields.push_back(tfield);
    dataReq.fields = fields;

    auto &conditionFields = dataReq.paraExt.fields;
    tfield.resetDefautlt();
    tfield.colValue = sBriefId;
    conditionFields["brief_id"] = tfield;

    dataproxy::TReadDataRsp dataRsp;
    iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(sBriefId)->redisRead(dataReq, dataRsp);
    if (iRet != 0 || dataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "get cowboy game banker detail err, iRet: " << iRet << ", iResult: " << dataRsp.iResult << endl;
        return -1;
    }
    for (auto it = dataRsp.fields.begin(); it != dataRsp.fields.end(); ++it)
    {
        map<string, string> mapGameRecord;
        for (auto itfield = it->begin(); itfield != it->end(); ++itfield)
        {
            mapGameRecord.insert(std::make_pair(itfield->colName, itfield->colValue));
        }
        vecGameRecord.push_back(mapGameRecord);
    }

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

int Processor::InsertGameDetailPineApple(const gamerecord::GameDetailPineApple& gameDetail)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    LOG_DEBUG << "GameDetailPineApple: "<< printTars(gameDetail)<< endl;

    if(gameDetail.sBriefId.empty())
    {
        ROLLLOG_ERROR << "report data err. sBriefId: "<< gameDetail.sBriefId << endl;
        return -1;
    }

    dataproxy::TWriteDataReq dataReq;
    dataReq.resetDefautlt();
    dataReq.keyName = I2S(E_REDIS_TYPE_HASH) + ":" + I2S(GAME_RECORD_DETAIL_PINEAPPLE) + ":" + gameDetail.sBriefId;
    dataReq.operateType = E_REDIS_INSERT;
    dataReq.clusterInfo.resetDefautlt();
    dataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    dataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_STRING;
    dataReq.clusterInfo.frageFactor = tars::hash<string>()(gameDetail.sBriefId);

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = E_NONE;
    tfield.colName = "brief_id";
    tfield.colType = STRING;
    tfield.colValue = gameDetail.sBriefId;
    fields.push_back(tfield);
    tfield.colName = "room_key";
    tfield.colType = STRING;
    tfield.colValue = gameDetail.sRoomKey;
    fields.push_back(tfield);
    tfield.colName = "round";
    tfield.colType = INT;
    tfield.colValue = I2S(gameDetail.iRound);
    fields.push_back(tfield);
    tfield.colName = "uid";
    tfield.colType = BIGINT;
    tfield.colValue = L2S(gameDetail.lPlayerId);
    fields.push_back(tfield);
    tfield.colName = "wins";
    tfield.colType = BIGINT;
    tfield.colValue = L2S(gameDetail.lWins);
    fields.push_back(tfield);

    Pb::GameDetailPineApple detail;
    detail.set_sbriefid(gameDetail.sBriefId);
    detail.set_sroomkey(gameDetail.sRoomKey);
    detail.set_sroomname(gameDetail.sRoomName);
    detail.set_iround(gameDetail.iRound);
    detail.set_lscore(gameDetail.lScore);
    detail.set_iplayercount(gameDetail.iPlayerCount);
    detail.set_lplayerid(gameDetail.lPlayerId);
    detail.set_lwins(gameDetail.lWins);

    for (auto item : gameDetail.userStatInfo)
    {
        auto pInfo = detail.add_userinfos();
        pInfo->set_lplayerid(item.second.lUid);
        pInfo->set_snickname(item.second.sNickname);
        pInfo->set_sheadstr(item.second.sHeadStr);
        pInfo->set_icid(item.second.iCid);
        pInfo->set_bbanker(item.second.bBanker);
        pInfo->set_bbomb(item.second.bBomb);
        pInfo->set_iftccount(item.second.iFTCCount);
        pInfo->set_lwins(item.second.lWins);
        pInfo->set_iscore(item.second.iScore);

        for (auto iitem : item.second.vCard)
        {
            auto pCard = pInfo->add_vcard();
            pCard->set_iflag(iitem.iFlag);
            pCard->set_iscore(iitem.iScore);
            pCard->set_icardtype(iitem.iCardType);

            for (auto card : iitem.vCard)
            {
                pCard->add_vcard(card);
            }
        }
    }
    
    string sDetail;
    detail.SerializeToString(&sDetail);
    tfield.colName = "detail";
    tfield.colType = STRING;
    tfield.colValue = sDetail;
    fields.push_back(tfield);

    dataReq.fields = fields;

    dataproxy::TWriteDataRsp dataRsp;
    iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(gameDetail.sBriefId)->redisWrite(dataReq, dataRsp);
    if (iRet != 0 || dataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "insert pineapple game detail err, iRet: " << iRet << ", iResult: " << dataRsp.iResult << endl;
        return -2;
    }

    ROLLLOG_DEBUG << "insert pineapple game detail, iRet: " << iRet << ", dataRsp: " << printTars(dataRsp) << endl;

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

int Processor::InsertGameRecordPineApple(const gamerecord::GameRecordPineApple& gameRecord)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    LOG_DEBUG << "InsertGameRecordPineApple: "<< printTars(gameRecord)<< endl;

    if(gameRecord.sBriefId.empty())
    {
        ROLLLOG_ERROR << "report data err. sBriefId: "<< gameRecord.sBriefId << endl;
        return -1;
    }

    dataproxy::TWriteDataReq dataReq;
    dataReq.resetDefautlt();
    dataReq.keyName = I2S(E_REDIS_TYPE_HASH) + ":" + I2S(GAME_RECORD_RECORD_PINEAPPLE) + ":" + gameRecord.sBriefId;
    dataReq.operateType = E_REDIS_INSERT;
    dataReq.clusterInfo.resetDefautlt();
    dataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    dataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_STRING;
    dataReq.clusterInfo.frageFactor = tars::hash<string>()(gameRecord.sBriefId);

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = E_NONE;
    tfield.colName = "brief_id";
    tfield.colType = STRING;
    tfield.colValue = gameRecord.sBriefId;
    fields.push_back(tfield);
    tfield.colName = "uid";
    tfield.colType = BIGINT;
    tfield.colValue = L2S(gameRecord.lPlayerId);
    fields.push_back(tfield);
    tfield.colName = "nickname";
    tfield.colType = STRING;
    tfield.colValue = gameRecord.sNickname;
    fields.push_back(tfield);
    tfield.colName = "head_str";
    tfield.colType = STRING;
    tfield.colValue = gameRecord.sHeadStr;
    fields.push_back(tfield);
    tfield.colName = "round";
    tfield.colType = INT;
    tfield.colValue = I2S(gameRecord.iRound);
    fields.push_back(tfield);
    tfield.colName = "buy_in";
    tfield.colType = BIGINT;
    tfield.colValue = I2S(gameRecord.lBuyIn);
    fields.push_back(tfield);
    tfield.colName = "buy_out";
    tfield.colType = BIGINT;
    tfield.colValue = I2S(gameRecord.lBuyOut);
    fields.push_back(tfield);
    tfield.colName = "wins";
    tfield.colType = BIGINT;
    tfield.colValue = I2S(gameRecord.lWins);
    fields.push_back(tfield);
    tfield.colName = "fees";
    tfield.colType = BIGINT;
    tfield.colValue = I2S(gameRecord.lFees);
    fields.push_back(tfield);

    dataReq.fields = fields;

    dataproxy::TWriteDataRsp dataRsp;
    iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(gameRecord.sBriefId)->redisWrite(dataReq, dataRsp);
    if (iRet != 0 || dataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "insert pineapple game record err, iRet: " << iRet << ", iResult: " << dataRsp.iResult << endl;
        return -2;
    }

    ROLLLOG_DEBUG << "insert pineapple game record, iRet: " << iRet << ", dataRsp: " << printTars(dataRsp) << endl;

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

int Processor::SelectGameRecordPineApple(const string sBriefId, vector<map<string, string>>& vecGameRecord)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    dataproxy::TReadDataReq dataReq;
    dataReq.resetDefautlt();
    dataReq.keyName = I2S(E_REDIS_TYPE_LIST) + ":" + I2S(GAME_RECORD_RECORD_PINEAPPLE) + ":" + sBriefId;
    dataReq.operateType = E_REDIS_READ;
    dataReq.clusterInfo.resetDefautlt();
    dataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    dataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_STRING;
    dataReq.clusterInfo.frageFactor = tars::hash<string>()(sBriefId);
    dataReq.paraExt.resetDefautlt();
    dataReq.paraExt.subOperateType = E_REDIS_LIST_RANGE;//根据范围取数据
    dataReq.paraExt.start = 0;//起始下标从0开始
    dataReq.paraExt.end = -1;//终止最大结束下标为-1

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = E_NONE;
    tfield.colName = "brief_id";
    fields.push_back(tfield);
    tfield.colName = "uid";
    fields.push_back(tfield);
    tfield.colName = "nickname";
    fields.push_back(tfield);
    tfield.colName = "heand_str";
    fields.push_back(tfield);
    tfield.colName = "round";
    fields.push_back(tfield);
    tfield.colName = "buy_in";
    fields.push_back(tfield);
    tfield.colName = "buy_out";
    fields.push_back(tfield);
    tfield.colName = "wins";
    fields.push_back(tfield);
    dataReq.fields = fields;
    tfield.colName = "fees";
    fields.push_back(tfield);
    dataReq.fields = fields;

    auto &conditionFields = dataReq.paraExt.fields;
    tfield.resetDefautlt();
    tfield.colValue = sBriefId;
    conditionFields["brief_id"] = tfield;

    dataproxy::TReadDataRsp dataRsp;
    iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(sBriefId)->redisRead(dataReq, dataRsp);
    if (iRet != 0 || dataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "get pineapple game record err, iRet:" << iRet << ", iResult:" << dataRsp.iResult << endl;
        return -1;
    }
    for (auto it = dataRsp.fields.begin(); it != dataRsp.fields.end(); ++it)
    {
        map<string, string> mapGameRecord;
        for (auto itfield = it->begin(); itfield != it->end(); ++itfield)
        {
            mapGameRecord.insert(std::make_pair(itfield->colName, itfield->colValue));
        }
        vecGameRecord.push_back(mapGameRecord);
    }

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

int Processor::SelectGameDetailPineApple(const string sBriefId, const int iRound, vector<map<string, string>>& vecGameDetail)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    dataproxy::TReadDataReq dataReq;
    dataReq.resetDefautlt();
    dataReq.keyName = I2S(E_REDIS_TYPE_LIST) + ":" + I2S(GAME_RECORD_DETAIL_PINEAPPLE) + ":" + sBriefId;
    dataReq.operateType = E_REDIS_READ;
    dataReq.clusterInfo.resetDefautlt();
    dataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    dataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_STRING;
    dataReq.clusterInfo.frageFactor = tars::hash<string>()(sBriefId);
    dataReq.paraExt.resetDefautlt();
    dataReq.paraExt.subOperateType = E_REDIS_LIST_RANGE;//根据范围取数据
    dataReq.paraExt.start = 0;//起始下标从0开始
    dataReq.paraExt.end = -1;//终止最大结束下标为-1

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = E_NONE;
    tfield.colName = "round";
    fields.push_back(tfield);
    tfield.colName = "uid";
    fields.push_back(tfield);
    tfield.colName = "wins";
    fields.push_back(tfield);
    tfield.colName = "detail";
    fields.push_back(tfield);
    dataReq.fields = fields;

    auto &conditionFields = dataReq.paraExt.fields;
    tfield.resetDefautlt();
    tfield.colValue = sBriefId;
    conditionFields["brief_id"] = tfield;

    dataproxy::TReadDataRsp dataRsp;
    iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(sBriefId)->redisRead(dataReq, dataRsp);
    if (iRet != 0 || dataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "get game detail pineapple err, iRet: " << iRet << ", iResult: " << dataRsp.iResult << endl;
        return -1;
    }
    for (auto it = dataRsp.fields.begin(); it != dataRsp.fields.end(); ++it)
    {
        map<string, string> mapGameDetail;
        for (auto itTField = it->begin(); itTField != it->end(); ++itTField)
        {
            mapGameDetail.insert(std::make_pair(itTField->colName, itTField->colValue));
        }

        if((S2I(mapGameDetail["round"]) == iRound && iRound >= 0) || iRound < 0)
        {
            vecGameDetail.push_back(mapGameDetail);
        }
    }

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;  
}