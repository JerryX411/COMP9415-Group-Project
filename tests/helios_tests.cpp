// Walk the shortened yard with ordinary game inputs. No warps, collision bypass,
// disabled hazards, forced door states or injected inventory are used in this run.
#include "core/RelayGame.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <optional>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
constexpr float dt=1.F/120;
constexpr int side=192, floors=12, count=side*side*floors;
int id(int x,int y,int z){return (y*side+z)*side+x;}
glm::vec3 point(int n){return {float(n%side)*.5F,float(n/(side*side)),float(n/side%side)*.5F};}
bool intersects(glm::vec3 p,float height,const relay::Bounds& b) {
  if(p.y>=b.max.y-.001F||p.y+height<=b.min.y+.001F)return false;
  const auto q=glm::clamp(p,b.min,b.max); const float dx=p.x-q.x,dz=p.z-q.z;
  return dx*dx+dz*dz<.705F*.705F;
}
struct Runner {
  relay::Game g;
  std::ofstream trace;
  int damage=0,steps=0;
  Runner(const char* file){g.reset(1);if(file)trace.open(file);}
  std::string where()const {auto p=g.player().position;std::ostringstream s;s<<" at "<<p.x<<','<<p.y<<','<<p.z<<" t="<<g.elapsed();return s.str();}
  void tick(relay::Input in={}) {
    if(trace)trace<<in.movement.x<<' '<<in.movement.y<<' '<<in.jumpPressed<<' '<<in.jumpHeld<<' '<<in.crouchHeld<<' '<<in.sprintHeld<<' '<<in.interactPressed<<'\n';
    const int before=g.player().health;
    g.update(dt,in);++steps;
    damage+=std::max(0,before-g.player().health);
    if(g.state()==relay::RunState::Lost)throw std::runtime_error("Lost route"+where());
    if(g.playerIntersectsScene())throw std::runtime_error("Player embedded in geometry"+where());
  }
  void idle(float t){for(int n=0;n<int(t/dt);++n)tick();}
  bool clear(glm::vec3 p,float height)const {
    if(g.world().collidesCylinder(p+glm::vec3(0,.005F,0),.705F,height-.01F))return false;
    for(const auto& door:g.doors())for(const auto& b:door.panels())if(intersects(p,height,b))return false;
    return true;
  }
  // A half-cell navigation grid with real standing/crouched headroom and door
  // collision. The controller below still has to execute every edge physically.
  std::vector<glm::vec3> path(glm::vec3 goal)const {
    std::vector<float> costs(count,1e9F);std::vector<int> prev(count,-1);std::vector<signed char> valid(count,-1);
    const auto traversable=[&](int n){
      if(valid[n]>=0)return bool(valid[n]);const auto p=point(n);
      bool ok=p.x>4.7F&&p.x<87.3F&&p.z>4.7F&&p.z<63.3F&&p.y>=1&&p.y<11&&
          g.world().cell(int(p.x),int(p.y)-1,int(p.z))&&clear(p,relay::Game::crouchHeight);
      valid[n]=ok?1:0;return ok;
    };
    const auto from=g.player().position;
    int start=id(int(std::round(from.x*2)),int(std::round(from.y)),int(std::round(from.z*2)));
    const auto h=[&](int n){const auto d=point(n)-goal;return std::sqrt(d.x*d.x+d.z*d.z)+std::abs(d.y)*1.2F;};
    using Entry=std::pair<float,int>;std::priority_queue<Entry,std::vector<Entry>,std::greater<Entry>> open;
    costs[start]=0;open.push({h(start),start});int found=-1;
    while(!open.empty()) {
      auto [f,n]=open.top();open.pop();if(f>costs[n]+h(n)+.001F)continue;
      const auto p=point(n);
      if(glm::distance(glm::vec2(p.x,p.z),glm::vec2(goal.x,goal.z))<.72F&&std::abs(p.y-goal.y)<.2F){found=n;break;}
      const int x=n%side,z=n/side%side,y=n/(side*side);
      for(int dz=-4;dz<=4;++dz)for(int dx=-4;dx<=4;++dx)if((dx||dz)&&dx*dx+dz*dz<=16)for(int dy=-4;dy<=1;++dy) {
        if(dy==0&&(std::abs(dx)>1||std::abs(dz)>1))continue;
        const int nx=x+dx,nz=z+dz,ny=y+dy;if(nx<1||nx>=side-1||nz<1||nz>=side-1||ny<1||ny>=floors)continue;
        const int m=id(nx,ny,nz);if(!traversable(m))continue;
        // Sample at the high end so a one-cell stair can be jumped; reject
        // diagonal corner cuts and overhead obstructions.
        const auto q=point(m);glm::vec3 mid=(p+q)*.5F;mid.y=std::max(p.y,q.y);
        if(dy<0&&!clear(q,float(-dy)+relay::Game::playerHeight))continue;
        if(dy>0&&!clear(p,float(dy)+relay::Game::playerHeight))continue;
        if(!clear(mid,dy>0?relay::Game::playerHeight:relay::Game::crouchHeight))continue;
        if(!clear(glm::mix(p,q,.25F)+glm::vec3(0,std::max(p.y,q.y)-glm::mix(p,q,.25F).y,0),relay::Game::crouchHeight)||!clear(glm::mix(p,q,.75F)+glm::vec3(0,std::max(p.y,q.y)-glm::mix(p,q,.75F).y,0),relay::Game::crouchHeight))continue;
        const float cost=costs[n]+std::sqrt(float(dx*dx+dz*dz))*.5F+std::abs(dy)*1.4F+(!clear(q,relay::Game::playerHeight)?.3F:0);
        if(cost<costs[m]){costs[m]=cost;prev[m]=n;open.push({cost+h(m),m});}
      }
    }
    if(found<0){int reached=0,best=start;glm::vec3 lo(999),hi(0);for(int n=0;n<count;++n)if(costs[n]<1e8F){++reached;lo=glm::min(lo,point(n));hi=glm::max(hi,point(n));if(h(n)<h(best))best=n;}auto p=point(best);std::cerr<<"No path: visited="<<reached<<" bounds="<<lo.x<<','<<lo.y<<','<<lo.z<<".."<<hi.x<<','<<hi.y<<','<<hi.z<<" closest="<<p.x<<','<<p.y<<','<<p.z<<'\n';return {};}
    std::vector<glm::vec3> result;for(int n=found;n!=start;n=prev[n])result.push_back(point(n));
    std::reverse(result.begin(),result.end());
    std::vector<glm::vec3> compact;
    for(const auto p:result){while(compact.size()>1){const auto a=compact.back()-compact[compact.size()-2],b=p-compact.back();if(std::abs(a.y)>.01F||std::abs(b.y)>.01F||glm::length(glm::cross(a,b))>.01F||glm::dot(a,b)<=0)break;compact.pop_back();}compact.push_back(p);}return compact;
  }
  void walk(const std::vector<glm::vec3>& route, bool hopGaps=false, std::optional<float> firstTakeoffX={}) {
    for(const auto target:route) {
      float best=1e9F;int stalled=0;
      bool jumped=false,arrived=false;
      for(int i=0;i<120*8;++i) {
        const auto p=g.player().position;const glm::vec2 delta(target.x-p.x,target.z-p.z);const float d=glm::length(delta);
        if(d<.15F&&std::abs(p.y-target.y)<.16F&&g.player().grounded){arrived=true;break;}
        const auto dir=delta/std::max(d,.001F);relay::Input in;
        in.movement=dir*std::min(1.F,d*3.F);in.jumpHeld=true;
        const auto ahead=p+glm::vec3(dir.x*.85F,0,dir.y*.85F);
        in.crouchHeld=!clear(p,relay::Game::playerHeight)||!clear(target,relay::Game::playerHeight)||(clear(ahead,relay::Game::crouchHeight)&&!clear(ahead,relay::Game::playerHeight));
        const bool atLip=hopGaps&&!g.world().cell(int(p.x),int(std::round(p.y))-1,int(p.z));
        const bool stepsUp=target.y>p.y+.35F||
          (!clear(ahead,relay::Game::playerHeight)&&clear(ahead+glm::vec3(0,1,0),relay::Game::playerHeight));
        in.sprintHeld=hopGaps&&d>1.F;
        // The first roof gap starts at a low opening. Take off after clearing
        // its lintel, while the game's coyote-time window is still open.
        const bool jump=firstTakeoffX
          ? !jumped&&p.x<=*firstTakeoffX&&g.player().coyoteTime>0
          : g.player().grounded&&(atLip||stepsUp);
        in.jumpPressed=!in.crouchHeld&&jump;
        jumped=jumped||in.jumpPressed;
        tick(in);
        const float dist=d+std::abs(p.y-target.y);
        if(dist<best-.015F){best=dist;stalled=0;}else if(++stalled>240)throw std::runtime_error("Stuck approaching "+std::to_string(target.x)+","+std::to_string(target.y)+","+std::to_string(target.z)+where());
      }
      if(!arrived)throw std::runtime_error("Waypoint timed out"+where());
      firstTakeoffX.reset();
    }
    idle(.12F);
  }
  bool go(glm::vec3 target) {auto route=path(target);if(route.empty()&&glm::distance(target,g.player().position)>.4F)return false;walk(route);return true;}
  void collect(int i) {
    auto target=g.cores().at(i).position-glm::vec3(0,.8F,0);
    if(!go(target))throw std::runtime_error("No route to core "+std::to_string(i)+where());
    pickUp(i);
  }
  void pickUp(int i) {
    if(g.nearestCore()!=i)throw std::runtime_error("Core out of reach"+where());
    relay::Input in;in.interactPressed=true;tick(in);idle(.5F);
    if(!g.cores().at(i).collected)throw std::runtime_error("Core pickup failed"+where());
    if(g.gateUnlocked()!=(g.collectedCount()==4))throw std::runtime_error("Exit must require exactly four cores");
    std::cout<<"Collected core "<<i<<where()<<std::endl;
  }
};
}
int main(int argc,char** argv) {
  try {
    Runner bot(argc>1?argv[1]:nullptr);
    if(bot.g.cores().size()!=4||bot.g.room().max.z!=64)throw std::runtime_error("Wrong yard layout");
    for(std::size_t i=0;i<bot.g.cores().size();++i){const auto p=bot.g.cores().at(i).position;std::cout<<"Core "<<i<<" at "<<p.x<<','<<p.y<<','<<p.z<<'\n';}
    if(bot.g.plates().size()!=4||bot.g.doors().size()!=4)
      throw std::runtime_error("Wrong Helios door and plate count");
    const auto openDoor=[&](std::size_t index) {
      const auto& plate=bot.g.plates().at(index);
      if(!bot.go(plate.position))
        throw std::runtime_error("Cannot reach plate "+std::to_string(index)+bot.where());
      bot.idle(.5F);
      if(!bot.g.doors().at(std::size_t(plate.door)).unlocked)
        throw std::runtime_error("Plate "+std::to_string(index)+" did not unlock its door"+bot.where());
      std::cout<<"Opened door "<<plate.door<<bot.where()<<std::endl;
    };
    openDoor(3);
    openDoor(0);
    bot.collect(2);
    if(!bot.go({34.5F,4,23.F}))throw std::runtime_error("Northwest platform inaccessible"+bot.where());
    // Traverse the raised stones and collect from the ledge beside the core.
    bot.walk({{29.5F,4,23.F},{25.5F,4,23.5F},{21.5F,4,23.5F},{17.5F,4,23.5F},
              {17.5F,4,19.5F},{19.5F,4,17.5F},{19.5F,4,16.8F}},true,32.22F);
    bot.pickUp(3);
    bot.walk({{19.5F,1,19.5F}});
    openDoor(1);
    bot.collect(1);
    openDoor(2);
    bot.collect(0);
    if(!bot.go({46,1,8}))throw std::runtime_error("Exit corridor inaccessible");
    for(int n=0;n<120*3&&bot.g.state()!=relay::RunState::Won;++n){relay::Input in;in.movement={0,-1};bot.tick(in);}
    if(bot.g.state()!=relay::RunState::Won||bot.g.collectedCount()!=4)throw std::runtime_error("Four-core finish failed"+bot.where());
    std::cout<<"Helios Yard full input run: "<<bot.g.elapsed()<<" seconds; cores=4/4; damage="<<bot.damage<<"; falls="<<bot.g.falls()<<"; state=Won; frames="<<bot.steps<<'\n';
  } catch(const std::exception& e){std::cerr<<"FAIL helios: "<<e.what()<<'\n';return 1;}
}
