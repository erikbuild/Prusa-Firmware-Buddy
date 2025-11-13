# High Level Analyzer
# For more information and documentation, please go to https://support.saleae.com/extensions/high-level-analyzer-extensions

from saleae.analyzers import HighLevelAnalyzer, AnalyzerFrame, StringSetting, NumberSetting, ChoicesSetting

B_REGISTERS_PREFIX = 0xFB
WRITE_FIFO_PREFIX = 0x80
READ_FIFO_PREFIX = 0x9F
COMMAND_MASK = 3 << 6
READ_REG_MASK = 1 << 6


# High level analyzers must subclass the HighLevelAnalyzer class.
class Hla(HighLevelAnalyzer):
    # List of settings that a user can set for this High Level Analyzer.
    # my_string_setting = StringSetting()
    # my_number_setting = NumberSetting(min_value=0, max_value=100)
    # my_choices_setting = ChoicesSetting(choices=('A', 'B'))

    # An optional list of types this analyzer produces, providing a way to customize the way frames are displayed in Logic 2.
    result_types = {
        'command': {
            'format': 'Command: {{data.command_id}}'
        },
        'read_register': {
            'format': 'ReadREG {{data.letter}}[{{data.range}}]: {{data.data}}'
        },
        'write_register': {
            'format': 'WriteREG {{data.letter}}[{{data.range}}]: {{data.data}}'
        },
        'read_fifo': {
            'format': 'ReadFIFO: {{data.fifo}}'
        },
        'write_fifo': {
            'format': 'WriteFIFO: {{data.fifo}}'
        },
    }

    def __init__(self):
        '''
        Initialize HLA.

        Settings can be accessed using the same name used above.
        '''
        self.frame_mosi = bytes()
        self.frame_miso = bytes()
        self.frame_start = 0

    @staticmethod
    def format_data(data: bytes) -> str:
        return ", ".join([f"0x{x:02x}" for x in data])

    def decode(self, frame: AnalyzerFrame):
        '''
        Process a frame from the input analyzer, and optionally return a single `AnalyzerFrame` or a list of `AnalyzerFrame`s.

        The type and data values in `frame` will depend on the input analyzer.
        '''

        if frame.type == 'enable':
            self.frame_start = frame.start_time
        elif frame.type == 'result':
            self.frame_mosi += frame.data["mosi"]
            self.frame_miso += frame.data["miso"]
        elif frame.type == 'disable':
            start_time = self.frame_start
            self.frame_start = 0
            mosi = self.frame_mosi
            self.frame_mosi = bytes()
            miso = self.frame_miso
            self.frame_miso = bytes()
            if len(miso) > 0 and len(mosi) == len(miso):
                first_byte = mosi[0]
                is_b_reg = False

                if first_byte == B_REGISTERS_PREFIX:
                    is_b_reg = True
                    first_byte = mosi[1]
                    miso = miso[1:]
                    mosi = mosi[1:]
                elif first_byte == WRITE_FIFO_PREFIX:
                    return AnalyzerFrame('write_fifo', start_time,
                                         frame.end_time,
                                         {'fifo': self.format_data(mosi[1:])})
                elif first_byte == READ_FIFO_PREFIX:
                    return AnalyzerFrame('read_fifo', start_time,
                                         frame.end_time,
                                         {'fifo': self.format_data(miso[1:])})
                elif (first_byte & COMMAND_MASK) == COMMAND_MASK:
                    return AnalyzerFrame('command', start_time, frame.end_time,
                                         {'command_id': f"0x{first_byte:02x}"})

                regs_len = len(miso) - 1
                assert (regs_len > 0)
                frame_type = 'write_register'
                data = mosi[1:]

                if (first_byte & READ_REG_MASK) == READ_REG_MASK:
                    frame_type = 'read_register'
                    first_byte &= ~READ_REG_MASK
                    data = miso[1:]

                if regs_len == 1:
                    reg_range = f"0x{first_byte:02x}"
                else:
                    reg_range = f"0x{first_byte:02x} - 0x{first_byte + regs_len - 1:02x}"

                return AnalyzerFrame(
                    frame_type, start_time, frame.end_time, {
                        'letter': "B" if is_b_reg else "A",
                        'range': reg_range,
                        'data': self.format_data(data),
                    })
        return None
