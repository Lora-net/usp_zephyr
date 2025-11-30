# SPDX-License-Identifier: BSD-3-Clause-Clear

if(SX126X_ENABLE_LR_FHSS)
  set(LR_FHSS_SRC_PATH "${LBM_SX126X_LIB_DIR}/"
    CACHE PATH "Path to folder containing LR-FHSS driver" FORCE)
endif()
