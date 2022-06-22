from flask import Flask, redirect, url_for, render_template, session, request
from flask_wtf import FlaskForm
from wtforms.fields import DateField, StringField, SelectField, TextAreaField, FileField, DateTimeField
from wtforms.validators import DataRequired
from wtforms import validators, SubmitField
from werkzeug.utils import secure_filename
import firebase_admin
from firebase_admin import db, credentials, initialize_app, storage
from urllib.request import urlopen
import pandas as pd
import datetime
from io import StringIO
import re
import os
from apscheduler.schedulers.background import BackgroundScheduler

app = Flask(__name__)

app.config['SECRET_KEY'] = '#$%^&*'

cred_obj = firebase_admin.credentials.Certificate("iotprojdb-firebase-adminsdk-q9c5k-113a48d6a7.json")
storage_path = 'iotprojdb.appspot.com'
firebase_path = 'https://iotprojdb-default-rtdb.europe-west1.firebasedatabase.app/'
default_app = firebase_admin.initialize_app(cred_obj, {'databaseURL':firebase_path, 'storageBucket': storage_path})


csv_path = "csv_reports/"
backup_path = "csv_backups/"
action_ref = db.reference("/action")
data_ref = db.reference("/data")
real_data_ref = db.reference("/real_data")
sensors_ref = db.reference("/sensors")
scheduler_ref = db.reference("/scheduler")
bucket = storage.bucket()
sensors_data = sensors_ref.get()
sensors_ids = list(sensors_data.keys())

class InfoForm(FlaskForm):
    startdate = DateField('', format='%Y-%m-%d', validators=(validators.DataRequired(),))
    enddate = DateField('-', format='%Y-%m-%d', validators=(validators.DataRequired(),))
    submit = SubmitField('Generate CSV')


class editSensorForm(FlaskForm):
    sensor = SelectField(u'Sensor ID', choices=sensors_ids, validate_choice=True)
    image = FileField(u'Image [jpg]')
    location = StringField(u'Location')
    description = TextAreaField(u'Description')
    submit = SubmitField('Save')


class SchedulerForm(FlaskForm):
    startdate = StringField('', validators=(validators.DataRequired(),))
    enddate = StringField('', validators=(validators.DataRequired(),))
    submit = SubmitField('Add')


def getCurrentTime():
	res = urlopen('http://just-the-time.appspot.com/')
	result = res.read().strip()
	result_str = result.decode('utf-8')
	day = result_str[8:10]
	month = result_str[5:7]
	year = result_str[2:4]
	hour = datetime.datetime.strptime(result_str[11:], "%H:%M:%S")
	hour += datetime.timedelta(hours=3)
	hour = hour.strftime("%H:%M:%S")
	to_ret = datetime.datetime.strptime(day + "-" + month + "-" + year + " " + hour, "%d-%m-%y %H:%M:%S")
	return to_ret


def generateDF(start_time, end_time, df, backup=False):
    data_ref = db.reference("/data")
    sensors_data = sensors_ref.get()
    data = data_ref.get()
    if not data:
        return
    for key, val in data.items():
        key_arr = key.split(" ")
        date, time, sensor_id = key_arr
        if backup:
            date = datetime.datetime.strptime(date+' '+time, "%d-%m-%y %H:%M:%S")
        else:
            date = datetime.datetime.strptime(date, "%d-%m-%y")
        # print(f"date: {date}, start_time: {start_time}, end_time {end_time}")
        if date < start_time or date > end_time:
            continue
        event = val['value']
        day = date.strftime("%d/%m/%y")
        location = sensors_data[sensor_id]["location"]
        if sensor_id.startswith('S'):
            sensor_type = 'Sonar'
        elif sensor_id.startswith('D'):
            sensor_type = 'OpenMV Camera'
        else: # sensor_id contains V
            sensor_type = 'Rpi Camera'
        # print("[DEBUG] sensor_id: ", sensor_id, ", event: ", event, "time: ", time, ", day: ", day, "sensor_type", sensor_type)
        df.loc[len(df.index)] = [sensor_id, sensor_type, location, event, time, day]
        if backup:
            data_ref.child(key).delete()


def backupDayData(day = None):   
    columns_titles = ["sensorID", "Type", "Location", "Event", "Time", "Day"]
    df = pd.DataFrame(columns=columns_titles)
    if not day:
        start_day = getCurrentTime()
        print(f"start_day: {start_day}")
    day = start_day + datetime.timedelta(days=1)
    start_day = start_day.replace(hour=0, minute=0, second=0)
    day = day.replace(hour=0, minute=0, second=0)
    print(f"backup in progress, start: {start_day} - end: {day}")
    generateDF(start_day, day, df, backup=True)
    start_day_str = start_day.strftime("%d_%m_%y")
    new_csv_filename = backup_path + "backup_" + start_day_str + ".csv"
    df.to_csv(new_csv_filename, index=False)
    # upload the file to the storage
    blob = bucket.blob(new_csv_filename)
    with open(new_csv_filename, 'rb') as f:
        blob.upload_from_file(f)
    blob.make_public()
    os.remove(new_csv_filename)
    print(blob.public_url)
    print("done")


def updateDB():
    action_data = action_ref.get()
    if action_data == "off":
        action_ref.set("on")
        print(f"updateDB - turning on")
        return "on"
    action_ref.set("off")
    print(f"updateDB - turning off")
    return "off"


def generateCSVAux(start_date, end_date):
    # transfer date value to readable integers
    start_date = datetime.datetime.strptime(start_date[:16], "%a, %d %b %Y")
    end_date = datetime.datetime.strptime(end_date[:16], "%a, %d %b %Y")
    curr_date = getCurrentTime()
    print("GenerateCSVAux")
    print(f"curr_date: {curr_date}, start_date: {start_date}, end_date: {end_date}")
    # validate the dates
    if start_date > end_date:
        return "Please enter a valid date range"
    if curr_date + datetime.timedelta(days=1) < end_date:
        return "Invalid end date... I am not god"
    blobs = list(bucket.list_blobs())
    blobs = bucket.list_blobs()
    inrange_blobs = []
    for blob in blobs:
        if not blob.name.startswith(backup_path+'backup_') or not blob.name[19:27]:
            continue
        print(f"blob.name: {blob.name} type: {type(blob.name)}")
        blob_start_date = datetime.datetime.strptime(blob.name[19:27], "%d_%m_%y") 
        if blob_start_date >= start_date and blob_start_date <= end_date:
            inrange_blobs.append(blob.public_url)
    if inrange_blobs:
        df = pd.concat(map(pd.read_csv, inrange_blobs), ignore_index=True)
    else:
        columns_titles = ["sensorID", "Type", "Location", "Event", "Time", "Day"]
        df = pd.DataFrame(columns=columns_titles)
    if curr_date-end_date <= datetime.timedelta(days=1):
        generateDF(end_date, end_date, df)
    # create the csv file
    start_date_str = start_date.strftime("%d_%m_%y")
    end_date_str = end_date.strftime("%d_%m_%y")
    new_csv_filename = csv_path + "report_" + start_date_str + "-" + end_date_str + ".csv"
    df.to_csv(new_csv_filename, index=False)
    # upload the file to the storage
    blob = bucket.blob(new_csv_filename)
    with open(new_csv_filename, 'rb') as f:
        blob.upload_from_file(f)
    blob.make_public()
    os.remove(new_csv_filename)
    # generate a download url and return it
    return blob.public_url

bg_sched = BackgroundScheduler(daemon=True, timezone="Asia/Jerusalem")
bg_sched.add_job(backupDayData, trigger='cron', hour='0')
bg_sched.start()

def schedUpdate(action):
    print(f"Sched Update to: {action}")
    action_data = action_ref.get()
    if action_data == "off" and action == 'on':
        action_ref.set("on")
    elif action_data == 'on' and action == 'off':
        action_ref.set("off")
    return

def addScheduleAux(start_date, end_date):
    try:
        start_date_p = datetime.datetime.strptime(start_date, "%d-%m-%y %H:%M:%S")
    except:
        return f"Invalid Start Date: {start_date}"
    try:
        end_date_p = datetime.datetime.strptime(end_date, "%d-%m-%y %H:%M:%S")
    except:
        return f"Invalid End Date: {end_date}"
    if start_date_p > end_date_p:
        return "Invalid Date Range"
    if getCurrentTime() > start_date_p:
        return f"Stale Start Date: {start_date}"
    scheduler_data = scheduler_ref.get() if scheduler_ref.get() else []
    insert_index = 0
    if scheduler_data:
        # check overlapping schedules
        for dates in scheduler_data:
            s, e = list(dates.items())[0]
            s_p = datetime.datetime.strptime(s, "%d-%m-%y %H:%M:%S")
            e_p = datetime.datetime.strptime(e, "%d-%m-%y %H:%M:%S")
            if (start_date_p > s_p and start_date_p < e_p) or (end_date_p > s_p and end_date_p < e_p):
                return f"Overlapping Dates: {s} to {e}"
            # insert the new schedule in order.
            if e_p < start_date_p:    # if we got a schedule later then the current end, add 1
                insert_index += 1
    scheduler_data.insert(insert_index, {start_date:end_date})
    scheduler_ref.set(scheduler_data)
    # add a background schedule to start and stop according to the new date
    if not bg_sched.get_job(id=start_date):
        bg_sched.add_job(schedUpdate, run_date=start_date_p, args=["on"], trigger='date', id=start_date)
    if not bg_sched.get_job(id=end_date):
        bg_sched.add_job(schedUpdate, run_date=end_date_p, args=["off"], trigger='date', id=end_date)


@app.route("/", methods=['GET','POST'])  # this sets the route to this page
def home():
    form = InfoForm()
    scheduler_data = scheduler_ref.get()
    action_data = action_ref.get()
    next_sched = sched_time = ''
    curr_time = getCurrentTime()
    if scheduler_data:
        changed = False
        for i in range(len(scheduler_data)):
            s_0, e_0 = list(scheduler_data[0].items())[0]
            s_p = datetime.datetime.strptime(s_0, "%d-%m-%y %H:%M:%S")
            e_p = datetime.datetime.strptime(e_0, "%d-%m-%y %H:%M:%S")
            if (curr_time > e_p):
                scheduler_data.pop(0)
                changed = True
            elif action_data == 'off':
                time_delta = curr_time - s_p
                sched_time = curr_time - time_delta
                sched_time.strftime('%Y/%d/%m %H:%M:%S')
                next_sched = f'System activation in: '
            elif action_data == 'on' and curr_time > s_p:
                time_delta = e_p - curr_time
                sched_time = curr_time + time_delta
                sched_time.strftime('%Y/%d/%m %H:%M:%S')                
                next_sched = f'System shutdown in: '
        if changed:
            scheduler_ref.set(scheduler_data)
    res = session['res'] if 'res' in session else ''
    if res:
        session.pop('res')
        # res = 'Please enter a valid date range'
    if form.validate_on_submit():
        session['startdate'] = form.startdate.data
        session['enddate'] = form.enddate.data
        return redirect('generateCSV')
    return render_template("index.html", val=action_data, form=form, res=res, next_sched=next_sched, 
                           sched_time=sched_time)


@app.route("/updateDB")
def update_db():
    res = updateDB()
    return redirect(url_for("home"))


@app.route("/generateCSV")
def generateCSV():
    startdate = session['startdate']
    enddate = session['enddate']
    msg = generateCSVAux(startdate, enddate)
    if msg:
        session['res'] = msg
        return redirect(url_for("home", res=msg))


@app.route("/status")
def getStatus():
    # make a dict of all of the sensors (for now hard coded list) as keys, val initialized to 0
    active_sensors = {'S-01':0, 'S-02':0, 'S-03':0,'S-04':0,'S-05':0,'S-06':0,'S-07':0,'S-08':0,'S-09':0,'S-10':0, 'D-01':0, 'V-01':0}
    # query the real_data from the realtime db
    real_data = real_data_ref.get()
    print("[DEBUG] real_data: ", real_data)
    data_sensors = []
    curr_time = getCurrentTime()
    active_delta = datetime.timedelta(minutes=3)    # time that passed until asensor is inactive
    for sensor in active_sensors.keys():
        if sensor not in real_data:
            continue
        if sensor.startswith('S'):
            if real_data[sensor]["value"] == 'YES':
                data_sensors.append("Someone is sitting")
            else:
                data_sensors.append("No one is sitting")
        elif sensor.startswith('D'):
            if real_data[sensor]["value"] == 'IN':
                data_sensors.append("Someone entered")
            else:
                data_sensors.append("Someone exited")
        else:
            data_sensors.append("Captured " + real_data[sensor]["value"] + " people")
        sensor_last_update_time = datetime.datetime.strptime(real_data[sensor]["time"], "%d-%m-%y %H:%M:%S")
        # print("[DEBUG] sensor_last_update_time - curr_time: ", sensor_last_update_time - curr_time)
        if (sensor_last_update_time + active_delta >= curr_time):
            if real_data[sensor]["value"] == "ERROR":
                active_sensors[sensor] = 2
            else:
                active_sensors[sensor] = 1
            print("[DEBUG]Sensor: '", sensor, "' is active")
        else:
            print("[DEBUG] Sensor: '", sensor, "' is not active")
            active_sensors[sensor] = 0
    # pass the list to the return
    # loop through it in the html with JINGA and update the active sensors
    return render_template("status.html", active_sensors=active_sensors, data_sensors=data_sensors)


@app.route("/sensors",  methods=['GET','POST'])
def getSensors():
    # get the sensors data from the firebase
    form = editSensorForm()
    sensors_data = sensors_ref.get()
    sensors_locations = [tmp_d['location'] for tmp_d in list(sensors_data.values())]
    sensors_descriptions = [tmp_d['description'] for tmp_d in list(sensors_data.values())]
    sensor_image_prefixes = [tmp_d['image'] for tmp_d in list(sensors_data.values())]
    if form.validate_on_submit():
        sensor_id = form.sensor.data
        sensor_image_prefix = sensors_data[sensor_id]['image']
        if form.image.data:
            uploaded_file = request.files[form.image.name]
            print(f"uploaded_file:  {uploaded_file}")
            uploaded_file.save('images/'+sensor_id + ".jpg")
            blob = bucket.blob('images/'+sensor_id+'_'+str(sensor_image_prefix)+'.jpg')
            with open('images/'+sensor_id + '.jpg', 'rb') as img:
                if blob.exists():
                    blob.delete()
                sensor_image_prefix += 1
                blob = bucket.blob('images/'+sensor_id+'_'+str(sensor_image_prefix)+'.jpg')
                print('images/'+sensor_id+'_'+str(sensor_image_prefix)+'.jpg')
                blob.upload_from_file(img, content_type='image/jpeg')
                blob.make_public()
            print(blob.public_url)
        s_location = form.location.data if form.location.data else sensors_data[sensor_id]['location']
        s_description = form.description.data if form.description.data else sensors_data[sensor_id]['description']
        new_data = {sensor_id: {'location':s_location, 'description':s_description, 'image':sensor_image_prefix}}
        sensors_ref.update(new_data)
        return redirect('sensors')
    return render_template("sensors.html", s_ids=sensors_ids, s_locations=sensors_locations, s_descriptions=sensors_descriptions,
                            sensor_image_prefixes=sensor_image_prefixes, form=form)


@app.route("/scheduler", methods=['GET','POST'])
def scheduler():
    form = SchedulerForm()
    scheduler_data = scheduler_ref.get()
    parsed_data = []
    if scheduler_data:
        changed = False
        curr_time = getCurrentTime()
        print(scheduler_data)
        for i in range(len(scheduler_data)):
            s, e = list(scheduler_data[i].items())[0]
            # remove stale data 
            e_time = datetime.datetime.strptime(e, "%d-%m-%y %H:%M:%S")
            if e_time < curr_time:
                scheduler_data.pop(i)
                changed = True
            else:
                parsed_data.append((i, s, e))
            if changed:
                scheduler_ref.set(scheduler_data)
    res = session['res'] if 'res' in session else ''
    if res:
        session.pop('res')
    if form.validate_on_submit():
        # validate date is ok within the scheduler_data
        session['startdate'] = form.startdate.data
        session['enddate'] = form.enddate.data
        return redirect('addSchedule')
    return render_template("scheduler.html", scheduler_data=parsed_data, form=form, res=res)


@app.route("/deleteSchedule")
def deleteSchedule():
    schedule_id = request.args.get('id')
    scheduler_data = scheduler_ref.get()
    start_end = scheduler_data.pop(int(schedule_id))
    scheduler_ref.set(scheduler_data)
    s, e = list(start_end.items())[0]
    if bg_sched.get_job(id=s):
        bg_sched.remove_job(id=s)
    if bg_sched.get_job(id=e):
        bg_sched.remove_job(id=e)
    return redirect('scheduler')


@app.route("/addSchedule")
def addSchedule():
    startdate = session['startdate']
    enddate = session['enddate']
    res = addScheduleAux(startdate, enddate)
    if res:
        session['res'] = res
        return redirect(url_for("scheduler", res=res))
    return redirect(url_for("scheduler", res=''))

@app.route("/restartSystem")
def restartSystem():
    refs = [db.reference("/reset1-5"), db.reference("/reset6-10"), db.reference("resetmv")]
    for ref in refs:
        ref.set("yes")
    return redirect(url_for("getStatus"))

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)
