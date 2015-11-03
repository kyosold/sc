<?php

include_once('common.php');
include_once('utils.php');

$mid = trim(avoid_sql($_POST['mid']));
$fuid = trim(avoid_sql($_POST['fuid']));
$fnick = trim(avoid_sql($_POST['fnick']));
$tuid = trim(avoid_sql($_POST['tuid']));
$img_wh = trim(avoid_sql($_POST['imgwh']));
$img_name = trim(avoid_sql($_FILES['jpgname']['name']));
$img_size = $_FILES['jpgname']['size'];

$tag_type = "CHAT";

$res = array();


if (strlen($mid) > 0 &&
	strlen($fuid) > 0 &&
	strlen($fnick) > 0 &&
	strlen($tuid) > 0 &&
	strlen($img_name) > 0 && 
	$img_size > 0
	) {

    try {

        $db = new PDO($PDO_DB_DSN, DB_USER, DB_PWD);
    
        $db->setAttribute(PDO::ATTR_CASE, PDO::CASE_LOWER); //设置属性
        $db->setAttribute(PDO::ATTR_ERRMODE,PDO::ERRMODE_EXCEPTION);

		$tpid = 0;

		// ----- 查询是否已经是好友 -----
		// 查询关系表
		$sql = "SELECT id, pid, status FROM sc_relationship WHERE myid = {$tuid} AND fid = {$fuid} LIMIT 1";
		$stmt = $db->prepare($sql);
		$stmt->execute();
		if ($row = $stmt->fetch(PDO::FETCH_ASSOC)) {
			$tpid = $row['pid'];
		} else {
			$res = show_info('fail', '对方不是你的好友');
			echo json_encode($res);
			return 0;	
		}


		// ----- 操作 -----
		// 1. 接收上传图片
		$jpg_file = DATA_PATH ."/". $tuid ."/queue/";
		// 目录hash
		$jpg_file .= date('Ymd')."/";	
		$jpg_file .= strtolower($img_name);

		$jpg_thumb_file = substr($jpg_file, 0, -4) .".thumb.jpg";
		if (!file_exists(dirname($jpg_file))) {
			// 创建目录
			if (mk_dir(dirname($jpg_file)) !== TRUE) {
				log_error("fail", "mkdir ". dirname($jpg_file) ." fail");
				$res = show_info('fail', '系统临时错误 5100');
				echo json_encode($res);
				return 1;
			}
		}
		
		if (move_uploaded_file($_FILES['jpgname']['tmp_name'], $jpg_file)) {
			// 生成缩略图
			$wh_list = explode("x", $img_wh);
			$img_w = $wh_list[0];
			$img_h = $wh_list[1];
			$img = new Imagick($jpg_file);
			$img->thumbnailimage($img_w, $img_h);
			file_put_contents($jpg_thumb_file, $img);
		
		} else {
			log_error("move_uploaded_file ". $_FILES['jpgname']['tmp_name'] ." to {$jpg_file} fail");
			
			$res = show_info('fail', '系统临时错误 5101');
			echo json_encode($res);
			return 1;
		}
		
		// 2. 写消息到数据库
		$img_st = lstat($jpg_file);
		$img_size = $img_st['size'];
		$fnickB64 = base64_encode($fnick);
		$jpg_url = str_replace(DATA_PATH, DATA_HOST, $jpg_thumb_file);

		$qid = write_queue_to_db($db, $mid, $tag_type, $fuid, $fnickB64, $tuid, "img", $jpg_url, $img_size, $img_wh);
		if ($qid == -1) {
			$res = show_info('fail', '系统出错');
			echo json_encode($res);
			return;
		}

		// 2. 给对方发送通知 
		$ret = send_notice_to_uid($fuid, $fnickB64, $tuid, $tpid, $tag_type, $qid);
		if ($ret != true) {
			log_info("send notice to tuid:{$tuid} tag_type:{$tag_type} fail");
        }   

		$res = show_info('succ', '处理成功');
		$res['qid'] = $qid;
		$res['queue_file'] = $jpg_url;
		
        echo json_encode($res);
		return;

	} catch (PDOException $e) {
		log_info("find user fail: ".$e->getMessage());

        $res = show_info('fail', $e->getMessage());
        echo json_encode($res);
        return 1;
	}



}

$res['status'] = 'fail';
$res['des'] = 'parameters error';

echo json_encode($res);



?>
