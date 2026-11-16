-- A supported existing journal field from the former large-own-fleet view.
local read,write=smudge.read_file,smudge.write_file;local saved={}
smudge.read_file=function(path,n,o)if saved[path]then return saved[path]:sub((o or 0)+1,(o or 0)+n)end;if path:match('%.dat$')then return nil end;return read(path,n,o)end
smudge.write_file=function(path,value)saved[path]=value;return true end
local M=dofile('core.lua');local g=M.new(123456);for row=1,5 do assert(M.action(g,1,row))end;g.show=1;assert(M.valid(g));assert(dofile('storage.lua').save('1|'..M.encode(g)))
local app=dofile('main.lua');on_init();on_draw();on_button('confirm',true);on_draw();local l=app.layout();assert(not l.own and app.state().show==0,'saved own-fleet page trapped resumed game');assert(app.state().a==g.a and app.state().b==g.b and app.state().sa==g.sa and app.state().sb==g.sb,'resuming UI flag changed fleets/shots')
local before=M.encode(app.state());on_touch(l.x+l.cell//2,l.y+l.cell//2,'tap');on_draw();on_touch(l.view.x+l.view.w//2,l.view.y+l.view.h//2,'tap');on_draw();assert(app.state().sb:sub(1,1)=='1'and app.state().turn==2,'imported journal cannot fire at drawn targets')
capture('import-own-view.pgm');assert(write('import-own-view.state','phase='..app.state().phase..' show='..app.state().show..' shots='..app.state().sb..'\n',false))

on_exit();print('Existing own-fleet journal reopens on usable native targets without changing fleets/shots')
