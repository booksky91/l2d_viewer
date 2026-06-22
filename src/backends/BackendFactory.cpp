#include "backends/ILive2DBackend.hpp"
#include "backends/MockBackend.hpp"
#include "backends/CubismNativeBackend.hpp"

namespace l2dgui
{

std::unique_ptr<ILive2DBackend> createBackend()
{
#if defined(L2D_USE_MOCK_BACKEND)
    return std::make_unique<MockBackend>();
#else
    return std::make_unique<CubismNativeBackend>();
#endif
}

} // namespace l2dgui
