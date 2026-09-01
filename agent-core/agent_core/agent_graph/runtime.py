from __future__ import annotations

from langgraph.graph import END,START,StateGraph

from .models import AgentState,GraphResult
from .nodes import (conversation_orchestrator,memory_analyst,mutation_proposal_finalize,normalize_input,
                    reminder_planner,repair_response,response_composer,response_verifier,
                    route_after_verify,safe_fallback,state_analyst,tool_executor)


class AgentGraphRuntime:
    def __init__(self)->None:
        builder=StateGraph(AgentState)
        builder.add_node("input_normalizer",normalize_input)
        builder.add_node("conversation_orchestrator",conversation_orchestrator)
        builder.add_node("memory_analyst",memory_analyst)
        builder.add_node("reminder_planner",reminder_planner)
        builder.add_node("state_analyst",state_analyst)
        builder.add_node("tool_executor",tool_executor)
        builder.add_node("response_composer",response_composer)
        builder.add_node("response_verifier",response_verifier)
        builder.add_node("response_repair",repair_response)
        builder.add_node("safe_fallback",safe_fallback)
        builder.add_node("mutation_proposal_finalize",mutation_proposal_finalize)
        builder.add_edge(START,"input_normalizer");builder.add_edge("input_normalizer","conversation_orchestrator")
        builder.add_edge("conversation_orchestrator","memory_analyst");builder.add_edge("memory_analyst","reminder_planner")
        builder.add_edge("reminder_planner","state_analyst");builder.add_edge("state_analyst","tool_executor")
        builder.add_edge("tool_executor","response_composer")
        builder.add_edge("response_composer","response_verifier")
        builder.add_conditional_edges("response_verifier",route_after_verify,{"commit":"mutation_proposal_finalize","repair":"response_repair","fallback":"safe_fallback"})
        builder.add_edge("response_repair","response_verifier");builder.add_edge("safe_fallback","mutation_proposal_finalize")
        builder.add_edge("mutation_proposal_finalize",END);self.graph=builder.compile()

    async def execute(self,request_id:str,trace_id:str,payload:dict)->GraphResult:
        state=await self.graph.ainvoke({"request_id":request_id,"trace_id":trace_id,"raw_payload":payload,
                                        "node_trace":[],"branch_trace":[]})
        draft=state["draft"]
        return GraphResult(request_id=request_id,trace_id=trace_id,body=draft.body,emotion=draft.emotion,
                           intent=state["intent"],risk=state["risk"],mutations=draft.mutations,
                           committed=False,memory_citations=[{"record_id":item.get("record_id"),"source_type":item.get("source_type"),"fact_type":item.get("fact_type"),"recorded_at":str(item.get("recorded_at","")),"confidence":item.get("confidence"),"reason":item.get("reasons",[])} for item in state.get("memories",[])],degraded=state.get("degraded",False),
                           error_code=state.get("error_code"),repair_count=state.get("repair_count",0),
                           node_trace=state["node_trace"],branch_trace=state["branch_trace"])

    def model_usage(self):
        from .nodes import model_adapter
        return model_adapter.last_usage
