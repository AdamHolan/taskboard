#ifndef AGENT_PROTOCOL_H
#define AGENT_PROTOCOL_H

#include <stddef.h>
#include <wchar.h>

#define AGENT_PROTOCOL_VERSION 1
#define AGENT_TEXT_CAP 512

typedef enum AgentOperation {
    AGENT_OP_NONE,
    AGENT_OP_TASK_CREATE,
    AGENT_OP_TASK_FIND,
    AGENT_OP_TASK_COMPLETE
} AgentOperation;

typedef struct AgentProposal {
    unsigned protocol_version;
    AgentOperation operation;
    wchar_t source_text[AGENT_TEXT_CAP];
    wchar_t preview[AGENT_TEXT_CAP];
    int validated;
} AgentProposal;

void agent_make_demo_proposal(const wchar_t *request, AgentProposal *proposal);
const wchar_t *agent_operation_name(AgentOperation operation);

#endif

