#include <stdio.h>
#include <wchar.h>

#include "agent_protocol.h"

int wmain(int argc, wchar_t **argv)
{
    AgentProposal proposal;
    const wchar_t *request = argc > 1 ? argv[1] : L"";
    agent_make_demo_proposal(request, &proposal);
    wprintf(L"Taskboard agent stub\n");
    wprintf(L"Protocol version: %u\n", proposal.protocol_version);
    wprintf(L"Operation: %ls\n\n%ls\n", agent_operation_name(proposal.operation), proposal.preview);
    return 0;
}

