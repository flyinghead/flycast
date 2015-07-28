// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function $(id) {
  return document.getElementById(id);
}

// Add event listeners after the NaCl module has loaded.  These listeners will
// forward messages to the NaCl module via postMessage()
function attachListeners() {

}

// Handle a message coming from the NaCl module.
function handleMessage(event) {
  console.log(event);
  $("status").textContent = event.data;
  $("log").textContent += event.data;

  if (!(event.data instanceof Array))
    return;
  if (event.data.length != 2)
    return;
}
