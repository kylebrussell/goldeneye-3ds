#ifndef GE_ORIGINAL_BOND_HEAD_UPDATE_H
#define GE_ORIGINAL_BOND_HEAD_UPDATE_H

#ifdef __cplusplus
extern "C" {
#endif

int ge_original_bond_head_update_tick(float percent_speed,
                                      float speed_sideways);
float ge_original_bond_head_breathing_value(void);

#ifdef __cplusplus
}
#endif

#endif
