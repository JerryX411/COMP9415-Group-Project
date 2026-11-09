#include "app/BenchmarkRun.h"
#include "app/SceneBenchmark.h"
#include "core/RelayGame.h"
#include "core/FollowCamera.h"
#include "graphics/Window.h"
#include "graphics/RelayRenderer.h"
#include "graphics/Screenshot.h"
#include "graphics/OpenGL.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <filesystem>
namespace craft::app {
int runLiteBenchmark(int level,const std::string& output) {
 graphics::Window window(1920,1080,"Lite rendering benchmark | 60 warm-up + 240 measured frames");
 window.setFramebufferSize(1920,1080);window.setVSync(false);
 relay::Game game;game.reset(level);game.warpPlayer(game.jumpPads()[0].position);
 for(int i=0;i<24;++i)game.update(1.F/120,{});
 relay::FollowCamera camera;camera.reset(game);relay::Renderer renderer;
 relay::RenderOptions options;options.customView=true;options.viewEye={20,17,38};options.viewTarget={20,1,19};
 std::ofstream file;
 if(!output.empty()){file.open(output);if(!file)throw std::runtime_error("Cannot write benchmark CSV: "+output);}
 std::ostream& report=output.empty()?std::cout:file;
 report<<"# GPU: "<<glGetString(GL_RENDERER)<<"; OpenGL: "<<glGetString(GL_VERSION)<<"\n";
 report<<"# Frozen: level="<<level<<"; world=96x96x96; pad launch after 24 steps at 1/120s; eye=20,17,38; target=20,1,19; FOV=50; near=0.1; far=300; normal_strength=1; skybox and particles included\n";
 report<<"repeat,mode,width,height,warmup,measured,mean_ms,triangles,draw_calls,solid_voxels,exposed_faces,particles\n";
 // Alternate order to reduce systematic warm-up/order bias; each row gets its own warm-up.
 for(int repeat=0;repeat<3;++repeat)for(int pass=0;pass<2;++pass){
  options.optimised=(repeat%2==0?pass==1:pass==0);
  renderer.prepare(game,options,0); // mesh build and static instance upload excluded
  SceneBenchmark timer;timer.start();
  while(!timer.snapshot().complete){
   window.pollEvents();if(window.shouldClose())throw std::runtime_error("Benchmark cancelled; results incomplete");
   if(window.framebufferSize()!=std::pair<int,int>{1920,1080})throw std::runtime_error("Benchmark framebuffer changed");
   timer.beginSceneFrame();renderer.draw(1920,1080,options,camera);(void)timer.endSceneFrame();
   if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("Benchmark OpenGL error");
   if(timer.snapshot().complete&&repeat==0&&!output.empty())
    relay::saveScreenshot(output+(options.optimised?".optimized.png":".baseline.png"),1920,1080);
   window.swapBuffers();
  }
  const auto result=timer.snapshot();const auto stats=renderer.stats();
  report<<repeat+1<<','<<(options.optimised?"optimized":"baseline")<<",1920,1080,"<<result.warmupFrames<<','<<result.measuredFrames<<','<<std::fixed<<std::setprecision(6)<<result.averageMilliseconds<<','<<stats.submittedTriangles<<','<<stats.drawCalls<<','<<stats.voxels<<','<<stats.visibleFaces<<','<<game.liveParticleCount()<<'\n';
  report.flush();if(!report)throw std::runtime_error("Failed to write benchmark results");
 }
 std::cout<<"benchmark_complete output="<<(output.empty()?"stdout":output)<<'\n';return 0;
}
}
