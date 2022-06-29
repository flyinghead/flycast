function cbStart()
  local s = flycast.state
  print("Game started: " .. s.media)
  print("Game Id: " .. s.gameId)
  print("Display: " .. s.display.width .. "x" .. s.display.height)
end

function cbPause()
  print("Game paused")
end

function cbResume()
  print("Game resumed")
  flycast.emulator.displayNotification("Lua rules", 2000)
  local maple = flycast.config.maple
  print("Maple 1: " .. maple.getDeviceType(1), 
	maple.getSubDeviceType(1, 1), maple.getSubDeviceType(1, 2))
end

function cbTerminate()
  print("Game terminated")
end

function cbLoadState()
  print("State loaded")
end

function cbVBlank()
--  print("vblank x,y=", flycast.input.getAbsCoordinates(1))
end

function cbOverlay()
  f = f + 1
  local ui = flycast.ui
  ui.beginWindow("Lua", 10, 10, 150, 0)
    ui.text("Hello world")
    ui.rightText(f)
    ui.bargraph(f / 3600)
    ui.button("Reset", function()
	    f = 0
	end)
    ui.text("Stick")
    local input = flycast.input
    local ax = input.getAxis(1, 1)
    local ay = input.getAxis(1, 2)
    ui.bargraph((ax + 128) / 255)
    ui.bargraph((ay + 128) / 255)
    input.releaseButtons(1, 0x8)
    ui.button("Test", function()
	    input.pressButtons(1, 0x8)
	end)
    ui.text("Mouse")
    local x, y = input.getAbsCoordinates(1)
    ui.bargraph(x / 640)
    ui.bargraph(y / 480)
  ui.endWindow()
end

flycast_callbacks = {
  start = cbStart,
  pause = cbPause,
  resume = cbResume,
  terminate = cbTerminate,
  loadState = cbLoadState,
  vblank = cbVBlank,
  overlay = cbOverlay
}

print("Callback set")
f = 0

flycast.emulator.startGame("pstone2.zip")
