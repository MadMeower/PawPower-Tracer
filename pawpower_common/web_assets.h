#pragma once
#include "esphome/components/web_server_base/web_server_base.h"
namespace pawcommon {
// Canonical Paw Ball implementation. Both dashboard entrypoints load this asset.
inline const char PAW_BALL_JS[] = R"PAWCOMMONJS(
(()=>{'use strict';
  function newPongMatch() {
    return {player:160,ai:160,x:320,y:160,vx:-240,vy:95,you:0,donut:0,wait:0.6,winner:''};
  }
  function stepPong(s, dt, target, sound=()=>{}) {
    if(s.winner)return;
    dt=Math.max(0,Math.min(dt,0.025));
    const clamp=y=>Math.max(34,Math.min(286,y));
    s.player=clamp(s.player+Math.max(-360*dt,Math.min(360*dt,target-s.player)));
    s.ai=clamp(s.ai+Math.max(-190*dt,Math.min(190*dt,(s.vx>0?s.y:160)-s.ai)));
    if(s.wait>0){s.wait=Math.max(0,s.wait-dt);return;}
    s.x+=s.vx*dt;s.y+=s.vy*dt;
    if(s.y<7){s.y=14-s.y;s.vy=Math.abs(s.vy);sound('wall');}
    if(s.y>313){s.y=626-s.y;s.vy=-Math.abs(s.vy);sound('wall');}
    const hit=(center,direction)=>{
      const angle=Math.max(-1,Math.min(1,(s.y-center)/34))*1.05;
      const speed=Math.min(430,Math.hypot(s.vx,s.vy)+18);
      s.vx=direction*speed*Math.cos(angle);s.vy=speed*Math.sin(angle);
      sound('hit');
    };
    if(s.vx<0 && s.x<=37 && s.x>=20 && Math.abs(s.y-s.player)<=39){s.x=37;hit(s.player,1);}
    if(s.vx>0 && s.x>=603 && s.x<=620 && Math.abs(s.y-s.ai)<=39){s.x=603;hit(s.ai,-1);}
    if(s.x< -7 || s.x>647){
      const playerPoint=s.x>647;
      if(playerPoint)s.you++;else s.donut++;
      if(s.you>=7 || s.donut>=7)s.winner=s.you>=7?'You win!':'Donut wins!';
      sound(s.donut>=7?'meow':playerPoint?'point':'miss');
      s.x=320;s.y=160;s.vx=playerPoint?240:-240;s.vy=(s.you+s.donut)%2?95:-95;s.wait=0.7;
    }
  }
  function createPongAudio(host) {
    const Audio=host.AudioContext || host.webkitAudioContext;
    let audio=null,enabled=true;
    const voices=new Set();
    function stop(){
      for(const voice of voices){try{voice.osc.stop();}catch(_){}voice.osc.disconnect();voice.gain.disconnect();}
      voices.clear();
    }
    return {
      available:!!Audio,
      get enabled(){return enabled;},
      setEnabled(value){enabled=value;if(!enabled)stop();},
      unlock(){
        if(!enabled || !Audio)return;
        try{
          if(!audio)audio=new Audio();
          if(audio.state==='suspended')audio.resume().catch(()=>{});
        }catch(_){/* Audio failure must never interrupt the dashboard or game. */}
      },
      tone(kind){
        if(!enabled || !audio || audio.state!=='running')return;
        try{
          const osc=audio.createOscillator(),gain=audio.createGain(),now=audio.currentTime;
          const meow=kind==='meow',duration=meow?0.8:kind==='hit'||kind==='wall'?0.055:0.18;
          osc.type=meow?'triangle':'square';
          osc.frequency.setValueAtTime(meow?420:kind==='hit'?520:kind==='wall'?340:kind==='point'?880:220,now);
          if(meow){
            // A cartoon "mee-ow": rise into the vowel, then glide down.
            osc.frequency.exponentialRampToValueAtTime(820,now+0.12);
            osc.frequency.exponentialRampToValueAtTime(640,now+0.32);
            osc.frequency.exponentialRampToValueAtTime(210,now+duration);
          }
          gain.gain.setValueAtTime(0,now);gain.gain.linearRampToValueAtTime(meow?0.075:0.035,now+(meow?0.04:0.004));
          if(meow)gain.gain.linearRampToValueAtTime(0.05,now+0.35);
          gain.gain.exponentialRampToValueAtTime(0.0001,now+duration);
          osc.connect(gain);gain.connect(audio.destination);
          const voice={osc,gain};voices.add(voice);
          osc.onended=()=>{voices.delete(voice);osc.disconnect();gain.disconnect();};
          osc.start(now);osc.stop(now+duration+0.01);
        }catch(_){}
      },
      stop
    };
  }
  function mountPong() {
    const panel=document.createElement('details');
    panel.className='history';panel.id='paw-pong';
    panel.innerHTML=`<summary>Paw Ball · Play against Donut</summary>
      <p class="note">First to 7. Move with your mouse or drag on the court. Keyboard: ↑ / ↓ or W / S while the court is focused.</p>
      <p id="pong-score" role="status" aria-live="polite">You 0 · Donut 0</p>
      <canvas id="pong-court" width="640" height="320" tabindex="0" role="img" aria-label="Paw Ball court. You control the left paddle; Donut controls the right."
        style="display:block;width:100%;max-width:640px;aspect-ratio:2;background:#091820;border:1px solid #38775c;border-radius:12px;touch-action:none">Paw Ball requires a browser with canvas support.</canvas>
      <div class="settings-actions"><button id="pong-play" type="button">Start game</button><button id="pong-reset" type="button">New game</button><button id="pong-sound" type="button" aria-pressed="true">Sound: on</button></div>
      <p class="note">A little fun while the batteries charge.</p>`;
    document.querySelector('main').insertBefore(panel,document.querySelector('footer'));
    const canvas=panel.querySelector('canvas'),ctx=canvas.getContext('2d');
    const play=panel.querySelector('#pong-play'),reset=panel.querySelector('#pong-reset'),score=panel.querySelector('#pong-score');
    const soundButton=panel.querySelector('#pong-sound'),audio=createPongAudio(window);
    if(!audio.available){soundButton.disabled=true;soundButton.textContent='Sound unavailable';soundButton.setAttribute('aria-pressed','false');}
    soundButton.onclick=()=>{
      audio.setEnabled(!audio.enabled);soundButton.textContent=audio.enabled?'Sound: on':'Sound: off';
      soundButton.setAttribute('aria-pressed',String(audio.enabled));
      if(audio.enabled)audio.unlock();
    };
    if(!ctx){play.disabled=true;reset.disabled=true;score.textContent='Canvas is unavailable in this browser.';return;}
    let state=newPongMatch(),running=false,frame=0,last=0,target=160,keys=new Set(),lastScore='';
    function draw(){
      ctx.fillStyle='#091820';ctx.fillRect(0,0,640,320);
      ctx.strokeStyle='#29434c';ctx.setLineDash([7,9]);ctx.beginPath();ctx.moveTo(320,12);ctx.lineTo(320,308);ctx.stroke();ctx.setLineDash([]);
      ctx.fillStyle='#64e5b0';ctx.fillRect(20,state.player-34,10,68);
      ctx.fillStyle='#f0ba65';ctx.fillRect(610,state.ai-34,10,68);
      ctx.fillStyle='#f4f7fa';ctx.beginPath();ctx.arc(state.x,state.y,7,0,Math.PI*2);ctx.fill();
      const text=`You ${state.you} · Donut ${state.donut}${state.winner?' · '+state.winner:''}`;
      if(text!==lastScore){score.textContent=text;lastScore=text;}
      if(!running || state.winner){
        ctx.fillStyle='rgba(9,24,32,0.8)';ctx.fillRect(120,125,400,70);
        ctx.fillStyle='#f4f7fa';ctx.font='22px system-ui';ctx.textAlign='center';
        ctx.fillText(state.winner || (state.you+state.donut?'Paused':'Ready to play?'),320,168);
      }
    }
    function pause(silence=true){running=false;cancelAnimationFrame(frame);frame=0;keys.clear();if(silence)audio.stop();play.textContent=state.winner?'Play again':'Resume game';draw();}
    function tick(now){
      if(!running)return;
      const elapsed=Math.min(Math.max((now-last)/1000,0),0.05);last=now;
      // Small steps prevent fast balls crossing a paddle between frames.
      const steps=Math.max(1,Math.ceil(elapsed/0.008)),dt=elapsed/steps;
      for(let i=0;i<steps;i++){
        if(keys.size)target=state.player+((keys.has('ArrowDown')||keys.has('s')?1:0)-(keys.has('ArrowUp')||keys.has('w')?1:0))*360*dt;
        stepPong(state,dt,target,kind=>audio.tone(kind));
      }
      if(state.winner){pause(false);return;}
      draw();frame=requestAnimationFrame(tick);
    }
    play.onclick=()=>{
      if(running){pause();return;}
      audio.unlock();
      if(state.winner){state=newPongMatch();target=160;}
      running=true;last=performance.now();play.textContent='Pause game';canvas.focus({preventScroll:true});frame=requestAnimationFrame(tick);
    };
    reset.onclick=()=>{pause();state=newPongMatch();target=160;play.textContent='Start game';draw();};
    const point=event=>{const box=canvas.getBoundingClientRect();target=Math.max(34,Math.min(286,(event.clientY-box.top)*320/box.height));};
    canvas.onpointerdown=event=>{event.preventDefault();canvas.focus({preventScroll:true});canvas.setPointerCapture(event.pointerId);point(event);};
    canvas.onpointermove=event=>{if(event.pointerType==='mouse'||canvas.hasPointerCapture(event.pointerId))point(event);};
    canvas.onpointerup=event=>{if(canvas.hasPointerCapture(event.pointerId))canvas.releasePointerCapture(event.pointerId);};
    canvas.onkeydown=event=>{const key=event.key.length===1?event.key.toLowerCase():event.key;if(['ArrowUp','ArrowDown','w','s'].includes(key)){event.preventDefault();keys.add(key);}};
    canvas.onkeyup=event=>{keys.delete(event.key.length===1?event.key.toLowerCase():event.key);};
    canvas.onblur=()=>keys.clear();
    panel.addEventListener('toggle',()=>{if(!panel.open)pause();});
    document.addEventListener('visibilitychange',()=>{if(document.hidden)pause();});
    window.addEventListener('blur',()=>{if(running)pause();});
    draw();
  }

globalThis.PawPowerCommon={newPongMatch,stepPong,createPongAudio,mountPong};
})();
)PAWCOMMONJS";
class CommonAssetHandler : public AsyncWebHandler {
 public:
  bool canHandle(AsyncWebServerRequest *request)const override {
    char url[AsyncWebServerRequest::URL_BUF_SIZE];
    return request->method()==HTTP_GET && request->url_to(url)=="/paw/common.js";
  }
  void handleRequest(AsyncWebServerRequest *request)override {
    request->send(200,"application/javascript",PAW_BALL_JS);
  }
};
inline void setup_web_assets(){
  static bool registered=false;
  if(!registered && esphome::web_server_base::global_web_server_base){
    esphome::web_server_base::global_web_server_base->add_handler(new CommonAssetHandler());
    registered=true;
  }
}
}
