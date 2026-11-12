#include "graphics/RelayModels.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <glm/gtc/matrix_transform.hpp>

namespace {
void check(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
bool near(const glm::mat4& a,const glm::mat4& b,float eps=.0002F) {
 for(int c=0;c<4;++c)for(int r=0;r<4;++r)if(std::abs(a[c][r]-b[c][r])>eps)return false;
 return true;
}
int node(const relay::ArticulatedModel& model,const char* name) {
 for(std::size_t i=0;i<model.nodes.size();++i)if(model.nodes[i].name==name)return int(i);
 throw std::runtime_error(std::string("Missing gate part: ")+name);
}
void unlock(relay::Game& game) {
 const auto cores=game.cores();
 for(std::size_t k=0;k<cores.size();++k) {
  for(int i=0;i<120&&game.player().interactionTime>0;++i)game.update(1.F/120,{});
  game.warpPlayer(cores[k].position-glm::vec3(0,.8F,0));
  relay::Input input;input.interactPressed=true;game.update(1.F/120,input);
  for(int i=0;i<120&&!game.cores()[k].collected;++i)game.update(1.F/120,{});
  check(game.cores()[k].collected,"Real core interaction must unlock the exit");
 }
 game.warpPlayer(game.gatePosition()+glm::vec3(0,0,3));
}
}

int main() {try {
 relay::SceneModels models;
 models.poseGate(0,false);
 auto gate=models.gate();
 const int left=node(gate,"left_panel"),right=node(gate,"right_panel"),frame=node(gate,"gate_root");
 const int handle=node(gate,"left_panel_handle"),grip=node(gate,"left_handle_grip");
 check(gate.nodes[handle].parent==left&&gate.nodes[grip].parent==handle,"Handle/grip must inherit the panel joint");
 const auto original=gate.transforms(glm::mat4(1));
 auto root=glm::rotate(glm::translate(glm::mat4(1),{6,3,-2}),.7F,{0,1,0});
 const auto moved=gate.transforms(root);
 for(std::size_t i=0;i<moved.size();++i)check(near(moved[i],root*original[i]),"Root rotation/translation must move every part");
 gate.nodes[left].offset.x-=.7F;gate.nodes[left].rotation.z=.2F;
 const auto local=gate.transforms(glm::mat4(1));
 check(near(local[right],original[right])&&near(local[frame],original[frame]),"Local panel motion must not move sibling or frame");
 check(!near(local[handle],original[handle])&&!near(local[grip],original[grip]),"Panel motion must propagate to handle and grip");
 const auto handleBefore=local[handle];
 gate.nodes[left].dimensions={7,9,3};
 check(near(gate.transforms(glm::mat4(1))[handle],handleBefore),"Mesh dimensions must not stretch child offsets");

 // The animated articulated model must actually be present in the game's draw list.
 relay::Game game;game.reset(0);unlock(game);
 check(game.gateUnlocked()&&game.gateOpen()<.02F,"Last pickup starts a closed-to-open animation");
 float previous=game.gateOpen();
 int changing=0;
 for(int step=0;step<110;++step) {
  game.update(1.F/120,{});
  const float open=game.gateOpen();
  check(open>=previous&&open<=1.F,"Door must open monotonically without overshoot");
  if(open>previous&&open<1.F)++changing;
  previous=open;
  std::vector<relay::Instance> scene;
  models.append(game,scene);
  const auto& posed=models.gate();
  std::vector<relay::Instance> expected;
  posed.append(glm::translate(glm::mat4(1),game.gatePosition()),expected);
  for(const auto& part:expected) {
   check(part.material.w>0,"Gate parts must use textured material surfaces");
   check(std::count_if(scene.begin(),scene.end(),[&](const auto& instance){return near(instance.transform,part.transform)&&instance.colour==part.colour;})==1,
         "Each hierarchical gate part must reach the actual scene exactly once");
  }
  // Check real scene collisions at the centre and newly revealed doorway edge.
  if(step%10==0) {
   const float innerEdge=1.6F*open;
   const auto safe=game.player().position;
   game.warpPlayer(game.gatePosition());
   check(game.playerIntersectsScene()==(innerEdge<relay::Game::playerRadius-.001F),"Visible opening and centre collision must agree");
   game.warpPlayer(safe);
  }
 }
 check(changing>50&&game.gateOpen()==1.F,"Opening must contain multiple intermediate frames and settle fully");
 check(std::abs(models.gate().nodes[handle].rotation.z)>.5F,"Handle must release locally during opening");
 relay::Input walk;walk.movement={0,-1};
 for(int i=0;i<240&&game.state()==relay::RunState::Playing;++i)game.update(1.F/120,walk);
 check(game.state()==relay::RunState::Won,"Animated exit must allow normal walking completion");
 game.reset(0);std::vector<relay::Instance> resetScene;models.append(game,resetScene);
 check(game.gateOpen()==0&&!game.gateUnlocked(),"Restart must close and lock the gate");
 check(near(models.gate().transforms(glm::mat4(1))[handle],original[handle]),"Restart must reset the handle's local pose");

 // Animation is based on simulation time, including variable frame durations.
 float progress[3]{};const float frameDt[3]={1.F/30,1.F/60,1.F/120};
 for(int trial=0;trial<3;++trial) {
  relay::Game other;other.reset(0);unlock(other);
  for(int i=0;i<int(std::round(.4F/frameDt[trial]));++i)other.update(frameDt[trial],{});
  progress[trial]=other.gateOpen();
 }
 check(std::abs(progress[0]-progress[2])<.0001F&&std::abs(progress[1]-progress[2])<.0001F,"Opening timing must be frame-rate independent");
 std::cout<<"Gate: root/local inheritance, child scaling isolation, scene integration, smooth animation, collisions, completion and reset passed.\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
