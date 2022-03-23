var token='initialValue';
var secret=configuration.secret;

const myCheckBoxes = [
  { name: 'light00', gpio: 16 },
  { name: 'light01', gpio: 5 },
  { name: 'light02', gpio: 4 },
  { name: 'light10', gpio: 0 },
  { name: 'light11', gpio: 2 },
  { name: 'light12', gpio: 14 },
  { name: 'light20', gpio: 12 },
  { name: 'light21', gpio: 13 },
  { name: 'light22', gpio: 1 }
];

var apiHost = configuration.apiHost;

function gefDefaultHeaders(){
return new Headers({
                    'Content-Type': 'application/json',
                    'Access-Control-Allow-Origin':'*',
                    'X-ApiKey': secretAPIKey(),
                   }
                  );
}

function message(id, content, type, target) {
  $('<div class="alert alert-'+type+'" id="'+id+'">'+content+'</div>').hide().insertBefore(target).slideDown('500', function() {
      setTimeout(function(){
          $('#'+id).slideUp(500, function(){ $(this).remove(); });
      }, 5000);
  });
}

function secretAPIKey(){
  var decryptedBytes = CryptoJS.AES.decrypt(secret, token);
  var decryptedMessage = decryptedBytes.toString(CryptoJS.enc.Utf8);
  return decryptedMessage;
}

function handleErrors(response) {
    if (!response.ok) {
        throw Error(response.statusText);
    }
    return response;
}

function changeSwitch(url, checkboxItem) {
  var newStatus = checkboxItem.checked == true ? "on" : "off";
  var data ={"id": getGPIOByID(checkboxItem.id), "status": newStatus};
  console.debug("Trying to fetch " + apiHost + url + " " +JSON.stringify(data))
  fetch(apiHost+url, {
    method: "POST", 
    headers: gefDefaultHeaders(),
    body: JSON.stringify(data)
  })
  .then(handleErrors)
  .then(response => {
    console.debug("Changed status [" + checkboxItem.id + "] ok");
  })
  .catch(error => {
    revertCheckboxStatus(checkboxItem);
    var errorMsg = error.message + ' API: '+ url;
    message('get', errorMsg, 'danger', $('#msgBox'));
    console.error(error);
  });
}

function get(url) {
  console.log("Trying to fetch " + url);
  fetch(apiHost+url, {
    method: "GET", 
    headers: gefDefaultHeaders()
  })
  .then(handleErrors)
  .then(response => console.log("ok") )
  .catch(error => {
    var errorMsg = error.message + ' API: '+ url;
    message('get', errorMsg, 'danger', $('#msgBox'));
    console.error(error);
  });
}

function saveConfig() {
  console.log("clicked saveConfig()");
  get("/config/saveMemoryToDisk");
}

function clearConfig() {
  console.log("clicked clearConfig()");
  get("/config/cleanDisk");
  get("/utils/restart");
}

function revertCheckboxStatus(checkboxItem) {
  if (checkboxItem.checked) {
    $("#"+checkboxItem.id).bootstrapToggle('off', true);
  } else {
    $("#"+checkboxItem.id).bootstrapToggle('on', true);
  }
}

function handleCheckbox(element) {
  console.debug("clicked handleCheckbox()");
  console.debug("new checkbox values: id="+element.id+" , checked="+element.checked);
  changeSwitch("/gpio/update", element);
}

function getGPIOByID(id) {
  var mappedCheckBox = myCheckBoxes.find( ({ name }) => name === id );
  console.debug('getGPIOByID() found ' + JSON.stringify(mappedCheckBox));
  if (mappedCheckBox == null) {
    throw 'No mapping found for ' + id;
  }
  return mappedCheckBox.gpio;
}

function reloadData() {
  var getConfigUrl = '/config/getFromMemory';
  console.log("Reload data " + getConfigUrl);
  
  fetch(apiHost+getConfigUrl, {
    method: "GET", 
    headers: gefDefaultHeaders()
  })
  .then(handleErrors)
  .then(response => response.json())
  .then(data => {
    console.log("API Result: " + JSON.stringify(data));
    // var data ={"2":"on","16":"off","5":"on"};
    myCheckBoxes.forEach(function(obj) { 
      var checkboxID = obj.name;
      var checkboxGpio = obj.gpio;
      console.debug('Fetched checkbox '+ checkboxID + ' gpio:' + checkboxGpio);
      currentStatusLightStatus = data[checkboxGpio];
      enableCheckbox(checkboxID);
      if (currentStatusLightStatus != null) {
        handleCheckboxStatus(checkboxID, currentStatusLightStatus);
      } else {
        console.warn('Mapping ' + obj.gpio + ' not found');
        handleCheckboxStatus(checkboxID, 'off');
      }
    });
  })
  .catch(error => {
    myCheckBoxes.forEach(function(obj) { 
      var checkboxID = obj.name;
      disableCheckbox(checkboxID);
    });
    var errorMsg = error.message + ' API: '+ getConfigUrl;
    message('reloadData', errorMsg, 'danger', $('#msgBox'));
    console.log(errorMsg);
  } );
}

function retrieveToken() {
  var url = '/config/getToken';
  fetch(apiHost+url, {
    method: "GET", 
    headers: {
      'Content-Type': 'application/json',
      'Access-Control-Allow-Origin':'*'
    }
  })
  .then(handleErrors)
  .then(response => response.json())
  .then(data => {
    console.log("ok " + JSON.stringify(data));
    token=data.key;
    reloadData();
  })
  .catch(error => {
    var errorMsg = error.message + ' API: '+ url;
    message('get', errorMsg, 'danger', $('#msgBox'));
    console.error(error);
  });
}

function handleCheckboxStatus(checkboxID, onOffState) {
  var isChecked = document.getElementById(checkboxID).checked;

  if (isChecked) {
    if (onOffState === 'on') {
      console.debug('Status not changed for ' + checkboxID + " "+ onOffState);
      return
    }
  } else {
    if (onOffState === 'off') {
      console.debug('Status not changed for ' + checkboxID + " "+ onOffState);
      return
    }
  }
  console.log('Set new status for ' + checkboxID + " "+ onOffState);
  $("#"+checkboxID).bootstrapToggle(onOffState, true);
}

function enableCheckbox(checkboxID) {
  var isDisabled = document.getElementById(checkboxID).disabled;
  if (isDisabled) {
    $("#"+checkboxID).bootstrapToggle('enable');
  }
}

function disableCheckbox(checkboxID) {
  var isDisabled = document.getElementById(checkboxID).disabled;
  if (!isDisabled) {
    $("#"+checkboxID).bootstrapToggle('disable');
  }
}

document.addEventListener('DOMContentLoaded', function() {
  retrieveToken();
  console.log('Page loaded');
}, false);
