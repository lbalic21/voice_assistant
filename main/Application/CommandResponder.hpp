#ifndef _Command_Responder_
#define _Command_Responder_

#include "tensorflow/lite/c/common.h"

// Called every time the results of an audio recognition run are available. The
// human-readable name of any recognized command is in the `found_command`
// argument, `score` has the numerical confidence, and `is_new_command` is set
// if the previous command was different to this one.
void RespondToCommand(int32_t current_time, const char* found_command,
                      uint8_t score, bool is_new_command);

#endif  /* _Command_Responder_ */
