#include "agent_protocol.h"

#include <stdio.h>
#include <string.h>

const wchar_t *agent_operation_name(AgentOperation operation)
{
    switch (operation) {
    case AGENT_OP_TASK_CREATE: return L"task.create";
    case AGENT_OP_TASK_FIND: return L"task.find";
    case AGENT_OP_TASK_COMPLETE: return L"task.complete";
    default: return L"none";
    }
}

void agent_make_demo_proposal(const wchar_t *request, AgentProposal *proposal)
{
    memset(proposal, 0, sizeof(*proposal));
    proposal->protocol_version = AGENT_PROTOCOL_VERSION;
    proposal->operation = AGENT_OP_TASK_CREATE;
    if (request) {
        wcsncpy(proposal->source_text, request, AGENT_TEXT_CAP - 1);
    }
    _snwprintf(proposal->preview, AGENT_TEXT_CAP - 1,
        L"PROPOSED TASK\r\n\r\n"
        L"Task       Waiting for local model\r\n"
        L"Category   Not inferred\r\n"
        L"Schedule   Not inferred\r\n\r\n"
        L"────────────────────────\r\n"
        L"Mock proposal · protocol %u\r\n"
        L"Nothing has been saved.",
        proposal->protocol_version);
    proposal->preview[AGENT_TEXT_CAP - 1] = L'\0';
}
