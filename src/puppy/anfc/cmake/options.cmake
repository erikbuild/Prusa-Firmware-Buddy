set(CAN_BUS_TYPE
    "PUB6"
    CACHE STRING "CAN bus configuration used by the device"
    )
define_enum_option(NAME CAN_BUS_TYPE VALUE ${CAN_BUS_TYPE} ALL_VALUES "PUB6;SLX;UART")

set(NFC_ST25R3919B_AWS_CONFIG
    "NO_AWS"
    CACHE STRING "Default Active Wave Shaping configuration used by ST25R3919B chip on the board"
    )
define_enum_option(
  NAME NFC_ST25R3919B_AWS_CONFIG VALUE ${NFC_ST25R3919B_AWS_CONFIG} ALL_VALUES
  "NO_AWS;SLOW;MEDIUM;FAST"
  )

set(NFC_ST25R3919B_MODULATION
    "100"
    CACHE STRING "Default ST25R3919B's modulation amplitude shift setting"
    )
define_int_option(NAME NFC_ST25R3919B_MODULATION VALUE ${NFC_ST25R3919B_MODULATION})

set(NFC_ST25R3919B_ENABLED_ANTENNAS
    "BOTH"
    CACHE STRING "Sets what antennas are used by default"
    )
define_enum_option(
  NAME NFC_ST25R3919B_ENABLED_ANTENNAS VALUE ${NFC_ST25R3919B_ENABLED_ANTENNAS} ALL_VALUES
  "ANTENNA_1;ANTENNA_2;BOTH"
  )

set(NFC_BOARD_HAS_LEFT_RIGHT_DETECTION_PIN
    NO
    CACHE
      BOOL
      "Sets if firmware should use left/right can names if the left/right detection pin is present"
    )
define_boolean_option(
  NFC_BOARD_HAS_LEFT_RIGHT_DETECTION_PIN ${NFC_BOARD_HAS_LEFT_RIGHT_DETECTION_PIN}
  )
