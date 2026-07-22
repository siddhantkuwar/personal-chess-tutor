# Local AI and Edge-AI Future

## Principle

The deterministic chess runtime remains authoritative. A model may improve language, retrieval, lesson sequencing, and personalization, but never defines legality, evaluation, or history.

## Boundary

```cpp
class ICoachingProvider {
public:
    virtual CoachingResponse explain(
        const CoachingRequest& request,
        std::stop_token stop
    ) = 0;
};
```

Implementations may include deterministic templates, local language model, and hybrid retrieval.

## Structured input

Position, played/best move, evaluation change, classification evidence, patterns, user level, personal recurrence, and requested style. Do not shovel raw databases into a prompt because context windows are not municipal landfills.

## Constraints

Apple Silicon, fully local option, selectable model, bounded memory, cancellation, streaming, and deterministic fallback.

## Future systems work

Quantization, Metal/MLX integration, mmap model loading, prompt caching, local retrieval, latency/memory profiling, constrained decoding, factuality evaluation, and distillation.

## Guardrails

Verify moves against legality, evaluation claims against stored analysis, label uncertainty, never upload silently, allow disabling, and store model version.
