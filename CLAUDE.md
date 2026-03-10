# CLAUDE.md - Project Development Guidelines

This file contains development rules and guidelines for the cadRenderer project. These rules help ensure consistent, maintainable, and high-quality development practices.

## Development Workflow Rules

1. **方案先行，批准后实施**
   - 在编写任何代码之前，请先描述你的方案并等待批准。
   - 如果需求不明确，在编写任何代码之前务必提出澄清问题。

2. **任务分解原则**
   - 如果一项任务需要修改超过 3 个文件，请先停下来，将其分解成更小的任务。

3. **代码质量保障**
   - 编写代码后，列出可能出现的问题，并建议相应的测试用例来覆盖这些问题。

4. **Bug修复流程**
   - 当发现 bug 时，首先要编写一个能够重现该 bug 的测试，然后不断修复它，直到测试通过为止。

5. **持续改进机制**
   - 每次我纠正你之后，就在 CLAUDE.md 文件中添加一条新规则，这样就不会再发生这种情况了。

6. **RenderingServer接口保护规则**
   - RenderingServer.h和RenderingServer.cpp所有定义的函数接口在没有用户允许的前提下不许自己修改
   - 必须保持与AR/MR引擎的接口一致性，确保代码可以顺利集成

7. **运行方式规则**
   - 程序通过bash run.sh来运行，所有测试和验证必须基于此运行方式
   - 确保run.sh脚本在任何修改后仍能正常工作

8. **代码整理效果一致性规则**
   - 当整理代码时，必须保证所有测试场景的运行效果一致
   - 任何重构或优化不能改变渲染输出结果

9. **效果修改审查规则**
   - 当用户要求修改渲染效果时，必须经过用户审查后才能实施
   - 提供详细的修改方案和预期效果对比

10. **目录结构使用规则**
    - 每次修改时先查看 DIRECTORY_STRUCTURE.md 文件，了解项目结构
    - 根据目录结构定位相关文件，减少上下文的代码量
    - 当参考Unity或其他外部代码时，先对照目录结构理解对应关系

11. **测试超时禁止规则**
    - 永远不要使用timeout命令来限制渲染程序的运行时间
    - 渲染测试需要等待程序自然完成，不能强制中断
    - 确保测试脚本让程序自然退出，检查"Program stopped"字样

## Project-Specific Guidelines

### Architecture
- This is a virtual-reality fusion rendering engine based on VulkanSceneGraph
- Follow the existing client-server architecture pattern
- Maintain separation between rendering logic (asset/) and interface layer (src/)
- Preserve RenderingServer interface compatibility with AR/MR engine at all times
- Maintain DIRECTORY_STRUCTURE.md as a current reference for project navigation

### Code Style
- Use consistent naming conventions as seen in existing codebase
- Follow VulkanSceneGraph patterns for ref_ptr usage and resource management
- Document complex rendering algorithms and shader interactions

### Testing
- Prioritize rendering correctness tests for virtual-reality fusion scenarios
- Test performance-critical paths (encoding, shadow mapping, IBL preprocessing)
- Validate cross-platform compatibility (Linux/Windows)
- Use DIRECTORY_STRUCTURE.md to locate test assets and configuration files

### Safety
- Never commit changes without explicit user request
- Confirm before executing destructive operations
- Verify rendering output quality after significant changes