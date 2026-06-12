#pragma once

#include "result.h"

namespace junqi {

enum class BattleResult {
    LEFT_WINS,   // 红方胜（默认左为红）
    RIGHT_WINS,  // 黑方胜（默认右为黑）
    DRAW,        // 平局（同归于尽）
    INVALID      // 识别失败，无法判定
};

class BattleJudge {
public:
    /// 根据左右两枚棋子的识别结果，判定对战胜负
    BattleResult judge(const PieceResult& left,
                       const PieceResult& right) const;

    /// 返回 BattleResult 的中文描述
    static const char* to_string(BattleResult r);
};

} // namespace junqi
