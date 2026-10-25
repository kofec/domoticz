extraHWValidateParams = function (data, validators) {
	var settings = {};
	try { settings = JSON.parse(data["extra"] || "{}"); } catch (e) { return false; }
	if (!settings.search_device_id)
		return false;
	if ([0, 1, 2].indexOf(parseInt(data["Mode1"], 10)) < 0)
		return false;
	if ([60, 300, 600, 900, 1800].indexOf(parseInt(data["Mode2"], 10)) < 0)
		return false;
	if ([0, 60, 120, 300, 600].indexOf(parseInt(data["Mode3"], 10)) < 0)
		return false;
	return [0, 1, 2, 3, 4].indexOf(parseInt(data["Mode4"], 10)) >= 0;
}

extraHWInitParams = function (data) {
	var settings = {};
	try { settings = JSON.parse(data["extra"] || "{}"); } catch (e) { settings = {}; }
	if (data["Mode1"] === "" || data["Mode1"] === 0) data["Mode1"] = 0;
	if (data["Mode2"] === "" || data["Mode2"] === 0) data["Mode2"] = 1800;
	if (data["Mode3"] === "") data["Mode3"] = 300;
	if (data["Mode4"] === "") data["Mode4"] = 0;
	$("#hardwarecontent #divextrahwparams #tuyalocalregion").val(data["Mode1"]);
	$("#hardwarecontent #divextrahwparams #tuyalocalsearch").val(settings.search_device_id || "");
	$("#hardwarecontent #divextrahwparams #tuyalocalcloudpoll").val(data["Mode2"]);
	$("#hardwarecontent #divextrahwparams #tuyalocallan").val(data["Mode3"]);
	$("#hardwarecontent #divextrahwparams #tuyalocalrefresh").val(settings.refresh_ids || "");
	$("#hardwarecontent #divextrahwparams #tuyalocaldebug").val(data["Mode4"]);
	$("#hardwarecontent #divremote").hide();
	$("#hardwarecontent #divlogin").show();
	$("#hardwarecontent #divlogin #lblusername span").text("Access ID");
	$("#hardwarecontent #divlogin #lblpassword span").text("Access Secret");
}

extraHWUpdateParams = function (validators) {
	var data = {};
	data["Mode1"] = $("#hardwarecontent #divextrahwparams #tuyalocalregion").val();
	data["Mode2"] = $("#hardwarecontent #divextrahwparams #tuyalocalcloudpoll").val();
	data["Mode3"] = $("#hardwarecontent #divextrahwparams #tuyalocallan").val();
	data["Mode4"] = $("#hardwarecontent #divextrahwparams #tuyalocaldebug").val();
	var searchDeviceID = $.trim($("#hardwarecontent #divextrahwparams #tuyalocalsearch").val());
	if (searchDeviceID === "") {
		ShowNotify($.t("Please enter a Search DeviceID!"), 2500, true);
		return false;
	}
	data["extra"] = JSON.stringify({
		search_device_id: searchDeviceID,
		refresh_ids: $.trim($("#hardwarecontent #divextrahwparams #tuyalocalrefresh").val())
	});
	if (!extraHWValidateParams(data, $.myglobals.Validators)) return false;
	return data;
}
