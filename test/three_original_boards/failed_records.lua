TEST_RECORD_FAILURE=true
dofile('fixture-gameplay.lua')
-- The game journal still saved while the optional UI record write failed.
local M=dofile('core.lua');local app=dofile('main.lua');on_init();on_draw();assert(app.state().status~=0)
local stats=app.records();assert(stats.wins+stats.losses+stats.draws==0,'unwritten history was invented')
on_exit()
