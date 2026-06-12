#include "junqi/battle_judge.h"

namespace junqi {

BattleResult BattleJudge::judge(const PieceResult& left,
                                 const PieceResult& right) const {
    // 任一棋子识别失败 → 无效
    if (left.character_id < 0 || right.character_id < 0) {
        return BattleResult::INVALID;
    }

    const int lid = left.character_id;
    const int rid = right.character_id;

    // 炸弹(11) 与任何棋子同归于尽
    if (lid == 11 || rid == 11) {
        return BattleResult::DRAW;
    }

    // 相同棋子 → 平局
    if (lid == rid) {
        return BattleResult::DRAW;
    }

    // 地雷(10) 判定：只有工兵(9) 能破雷
    if (lid == 10) {
        return (rid == 9) ? BattleResult::RIGHT_WINS
                          : BattleResult::LEFT_WINS;
    }
    if (rid == 10) {
        return (lid == 9) ? BattleResult::LEFT_WINS
                          : BattleResult::RIGHT_WINS;
    }

    // 军旗(12) 被任何棋子吃
    if (lid == 12) return BattleResult::RIGHT_WINS;
    if (rid == 12) return BattleResult::LEFT_WINS;

    // 普通比大小：ID 越小军衔越高，强者胜
    return (lid < rid) ? BattleResult::LEFT_WINS
                       : BattleResult::RIGHT_WINS;
}

const char* BattleJudge::to_string(BattleResult r) {
    switch (r) {
        case BattleResult::LEFT_WINS:  return "红方获胜";
        case BattleResult::RIGHT_WINS: return "黑方获胜";
        case BattleResult::DRAW:       return "平局（同归于尽）";
        case BattleResult::INVALID:    return "判定无效";
        default:                       return "未知";
    }
}

} // namespace junqi
