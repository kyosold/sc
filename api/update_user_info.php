<?php

include_once('common.php');
include_once('utils.php');


$uid = trim(avoid_sql($_POST['uid']));
$nickname = trim(avoid_sql($_POST['nickname']));
$gender = trim(avoid_sql($_POST['gender']));
$birthday = trim(avoid_sql($_POST['birthday']));


$res = array();

if ((strlen($uid) > 0) &&
	(strlen($gender) > 0) &&
	(strlen($nickname) > 0) &&
	(strlen($birthday) > 0)) {

	try {
		$db = new PDO($PDO_DB_DSN, DB_USER, DB_PWD);

		$db->setAttribute(PDO::ATTR_CASE, PDO::CASE_LOWER); //设置属性
		$db->setAttribute(PDO::ATTR_ERRMODE,PDO::ERRMODE_EXCEPTION);
		
		// 注册用户信息
		$sql = "UPDATE sc_user SET nickname='{$nickname}', birthday='{$birthday}', gender={$gender} WHERE id = {$uid} LIMIT 1";

		$stmt = $db->prepare($sql);
		if (!$stmt->execute()) {
			throw new PDOException('系统出错(2001)，请稍候重试 :)');
		}
			
	} catch (PDOException $e) {

		log_info("update user information fail, ".$e->getMessage());

		$res = show_info('fail', $e->getMessage());
		$res['localdes'] = $e->getMessage();
		echo json_encode($res);

		return 0;
	}


		//----- 上传头像 -----
		$avatar_path = DATA_PATH ."/". $uid ."/avatar/";
		$avatar_img = $avatar_path ."avatar.jpg";
		$avatar_thumb = $avatar_path ."s_avatar.jpg";

		unlink($avatar_img);
		unlink($avatar_thumb);

	
		// 创建用户目录
		if (mk_dir($avatar_path) != TRUE) {
			my_rollback($db, $uid, $pid);

			log_info("update user information fail, mkdir fail:{$avatar_path}");
			$res = show_info('fail', '系统出错(4004)，请稍候重试 :)');
			$res['localdes'] = $avatar_path;
			echo json_encode($res);

			return 1;
		}

		// 检查头像是否有数据,有的话写文件
		$avatar_name = strtolower($_FILES['avatar']['name']);
		$avatar_size = $_FILES['avatar']['size'];

		if (strlen($avatar_name) > 0 && $avatar_size > 0) {
			if (move_uploaded_file($_FILES['avatar']['tmp_name'], $avatar_img)) {
				$img = new Imagick($avatar_img);
				$img->thumbnailImage(100, 100);	
				file_put_contents($avatar_thumb, $img);

				//log_info("upload avatar succ");

			} else {
				log_info("register fail, move_uploaded_file ".$_FILES['avatar']['tmp_name']." to {$avatar_img} fail");

				my_rollback($db, $uid, $pid);
				unlink($avatar_path);
				//log_info("unlink succ:{$avatar_path}");

				$res = show_info('fail', '上传头像失败');
				echo json_encode($res);
				return 1;
			}
		} else {
			log_info("register fail, avatar name or size less than 0");

			my_rollback($db, $uid, $pid);
			unlink($avatar_path);
			//log_info("unlink succ:{$avatar_path}");

			$res = show_info('fail', '上传头像失败');
			$res['localdes'] = 'avatar name or size less than 0';
			echo json_encode($res);
			return 1;
		}


		//不管执行成功还是失败最后都要在关闭自动提交
		$db->setAttribute(PDO::ATTR_AUTOCOMMIT, 1);

		log_info("register succ. uid:{$uid}");

		$res = show_info('succ', '注册成功!');
    	echo json_encode($res);
		return 0;
	
} else {
	log_info("register fail, parameters error");

	$res['status'] = 'fail';
	$res['des'] = 'parameters error';

	echo json_encode($res);
}

function my_rollback($db, $uid, $pid)
{
	$sql = "DELETE FROM sc_user WHERE id = :uid LIMIT 1";

	$stmt = $db->prepare($sql);
	$stmt->bindParam(':uid', $uid, PDO::PARAM_INT);
	$stmt->execute();
	log_info("delete from sc_user succ. id:{$uid}");
			
	$sql = "DELETE FROM sc_pwds WHERE id = :pid LIMIT 1";

	$stmt = $db->prepare($sql);
	$stmt->bindParam(':pid', $pid, PDO::PARAM_INT);
	$stmt->execute();
	log_info("delete from sc_pwds succ. id:{$pid}");

}




?>
