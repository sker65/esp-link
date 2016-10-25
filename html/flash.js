//===== FLASH cards

function flashFirmware(e) {
  e.preventDefault();
  var fw_data = document.getElementById('fw-file').files[0];
      
  domselect("#fw-form").setAttribute("hidden", "");
  domselect("#fw-spinner").removeAttribute("hidden");
  showNotification("Firmware is being updated ...");

  ajaxReq("POST", "/flash/upload", function (resp) {
    ajaxReq("GET", "/flash/reboot", function (resp) {
      showNotification("Firmware has been successfully updated!");
      setTimeout(function(){ window.location.reload()}, 4000);

      domselect("#fw-spinner").setAttribute("hidden", "");
      domselect("#fw-form").removeAttribute("hidden");
    });
  }, null, fw_data)
}

function fetchFlash() {
  ajaxReq("GET", "/flash/next", function (resp) {
    domselect("#fw-slot").innerHTML = resp;
    domselect("#fw-spinner").setAttribute("hidden", "");
    domselect("#fw-form").removeAttribute("hidden");
  });
  ajaxJson("GET", "/menu", function(data) {
      var v = domselect("#current-fw");
      if (v != null) { v.innerHTML = data.version; }
    }
  );
}
